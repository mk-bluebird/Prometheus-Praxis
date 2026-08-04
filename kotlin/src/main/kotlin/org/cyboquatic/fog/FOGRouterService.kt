// File: kotlin/src/main/kotlin/org/cyboquatic/fog/FOGRouterService.kt
package org.cyboquatic.fog

import java.sql.Connection
import java.sql.DriverManager
import java.sql.ResultSet
import java.time.Instant
import java.time.Duration

// Simple data class for FOG routing request.
data class FOGRequest(
    val nodeId: String,
    val canalId: String,
    val timestampMs: Long
)

// Route decision with KER-weighted confidence.
data class FOGRouteDecision(
    val route: String,          // PRIMARY_CANAL, SECONDARY_CANAL, HOLD_TANK
    val kerK: Double,           // knowledge factor
    val kerE: Double,           // eco-impact (negative is better)
    val kerR: Double,           // risk factor
    val confidence: Double      // 0..1
)

/**
 * Kotlin FOG-router micro-service with:
 *  - gRPC request handling (conceptual).
 *  - SQL lookup in cyboquatic_workload_telemetry for recent PFAS/turbidity.
 *  - Lua fallback via LuaJ when data is stale, using default approximations.
 */
object FOGRouterService {

    private const val JDBC_URL = "jdbc:sqlite:./data/cyboquatic_workload.db"
    private val STALE_THRESHOLD = Duration.ofMinutes(15)

    // LuaJ integration: load Lua script that exposes route_from_row(pfas, turbidity, organicFraction).
    private val luaGlobals = org.luaj.vm2.lib.jse.JsePlatform.standardGlobals()
    init {
        val script = luaGlobals.loadfile("lua/fog_router_predicates.lua")
        script.call()
    }

    fun handleRequest(request: FOGRequest): FOGRouteDecision {
        DriverManager.getConnection(JDBC_URL).use { conn ->
            conn.autoCommit = false
            val row = queryLatestTelemetry(conn, request.nodeId, request.canalId)
            return if (row != null && !isStale(row.timestampMs, request.timestampMs)) {
                val route = decideFromTelemetry(row)
                val ker = computeKerFromTelemetry(row)
                FOGRouteDecision(
                    route = route,
                    kerK = ker.first,
                    kerE = ker.second,
                    kerR = ker.third,
                    confidence = 0.9
                )
            } else {
                val fallbackDecision = decideFromLuaFallback()
                val kerFallback = computeKerFallback()
                FOGRouteDecision(
                    route = fallbackDecision,
                    kerK = kerFallback.first,
                    kerE = kerFallback.second,
                    kerR = kerFallback.third,
                    confidence = 0.6
                )
            }
        }
    }

    private data class TelemetryRow(
        val timestampMs: Long,
        val pfasUgL: Double,
        val turbidityNTU: Double,
        val organicFraction: Double
    )

    private fun queryLatestTelemetry(conn: Connection, nodeId: String, canalId: String): TelemetryRow? {
        val sql = """
            SELECT wt.timestamp_s, wt.energyreq_j, wt.delta_vt_m_s,
                   ext.pfas_ug_l, ext.turbidity_ntu, ext.organic_fraction
            FROM cyboquatic_workload_telemetry wt
            JOIN fog_media_ext ext ON wt.basin_id = ext.basin_id
            WHERE ext.node_id = ? AND ext.canal_id = ?
            ORDER BY wt.timestamp_s DESC
            LIMIT 1
        """.trimIndent()
        conn.prepareStatement(sql).use { ps ->
            ps.setString(1, nodeId)
            ps.setString(2, canalId)
            ps.executeQuery().use { rs ->
                return if (rs.next()) {
                    val tsSeconds = rs.getDouble("timestamp_s")
                    TelemetryRow(
                        timestampMs = (tsSeconds * 1000.0).toLong(),
                        pfasUgL = rs.getDouble("pfas_ug_l"),
                        turbidityNTU = rs.getDouble("turbidity_ntu"),
                        organicFraction = rs.getDouble("organic_fraction")
                    )
                } else null
            }
        }
    }

    private fun isStale(dataTimestampMs: Long, requestTimestampMs: Long): Boolean {
        val dataInstant = Instant.ofEpochMilli(dataTimestampMs)
        val requestInstant = Instant.ofEpochMilli(requestTimestampMs)
        return Duration.between(dataInstant, requestInstant) > STALE_THRESHOLD
    }

    private fun decideFromTelemetry(row: TelemetryRow): String {
        // Simple rule: high PFAS or turbidity -> HOLD_TANK.
        return if (row.pfasUgL > 0.5 || row.turbidityNTU > 200.0) {
            "HOLD_TANK"
        } else if (row.organicFraction > 0.5) {
            "PRIMARY_CANAL"
        } else {
            "SECONDARY_CANAL"
        }
    }

    private fun computeKerFromTelemetry(row: TelemetryRow): Triple<Double, Double, Double> {
        // Example KER mapping:
        val kerK = 0.9  // high knowledge: recent telemetry
        val kerE = if (row.pfasUgL < 0.5) -1.0 else -0.3  // more negative when PFAS low
        val kerR = if (row.pfasUgL < 0.5 && row.turbidityNTU < 200.0) 0.2 else 0.5
        return Triple(kerK, kerE, kerR)
    }

    private fun decideFromLuaFallback(): String {
        val viscosityCP = 20.0
        val turbidityNTU = 150.0
        val organicFraction = 0.4
        val fogRouter = luaGlobals.get("FOGRouter")
        val classify = fogRouter.get("classify_media")
        val score = fogRouter.get("predicate_score")
        val routeSuggest = fogRouter.get("suggest_route")

        val decision = classify.call(
            org.luaj.vm2.LuaValue.valueOf(viscosityCP),
            org.luaj.vm2.LuaValue.valueOf(turbidityNTU),
            org.luaj.vm2.LuaValue.valueOf(organicFraction)
        ).tojstring()

        val predScore = score.call(
            org.luaj.vm2.LuaValue.valueOf(viscosityCP),
            org.luaj.vm2.LuaValue.valueOf(turbidityNTU),
            org.luaj.vm2.LuaValue.valueOf(organicFraction)
        ).todouble()

        val route = routeSuggest.call(
            org.luaj.vm2.LuaValue.valueOf(predScore),
            org.luaj.vm2.LuaValue.valueOf(0.08) // assumed canal capacity
        ).tojstring()

        // Prefer HOLD_TANK if Lua suggests BLOCK or predicate_score is low.
        return when (decision) {
            "BLOCK" -> "HOLD_TANK"
            "SAFE"  -> route
            "CAUTION" -> if (predScore >= 0.7) route else "HOLD_TANK"
            else -> "HOLD_TANK"
        }
    }

    private fun computeKerFallback(): Triple<Double, Double, Double> {
        // Lower knowledge, more conservative risk.
        val kerK = 0.7
        val kerE = -0.8
        val kerR = 0.4
        return Triple(kerK, kerE, kerR)
    }
}
