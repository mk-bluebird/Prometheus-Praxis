// File: kotlin/cyboquatic/FogRouterPredicates.kt
package cyboquatic

import java.sql.DriverManager
import java.util.Properties

data class MediaSample(
    val mediumType: String,      // e.g. "sediment", "water", "foam"
    val temperatureC: Double,
    val pfasConcentrationUgL: Double,
    val dissolvedO2MgL: Double,
    val turbidityNTU: Double
)

object FogRouterPredicates {

    fun isColdSurvivalCorridor(media: MediaSample): Boolean {
        val isCold = media.temperatureC <= 12.0
        val highPFAS = media.pfasConcentrationUgL >= 0.1
        return isCold && highPFAS
    }

    fun isEcoRestorationReady(media: MediaSample): Boolean {
        val o2Safe = media.dissolvedO2MgL >= 5.0
        val turbiditySafe = media.turbidityNTU <= 50.0
        val pfasLow = media.pfasConcentrationUgL < 0.05
        return o2Safe && turbiditySafe && pfasLow
    }

    fun route(media: MediaSample): String {
        return when {
            isColdSurvivalCorridor(media) ->
                "FOG:COLD_SURVIVAL_MONITOR"
            isEcoRestorationReady(media) ->
                "FOG:RESTORATION_PREFERRED"
            else ->
                "FOG:NEEDS_DIAGNOSTIC"
        }
    }
    
    /**
     * Query latest telemetry row from SQLite and convert to MediaSample for routing.
     * Uses JDBC with the Xerial SQLite driver (no extra libraries needed).
     */
    fun decideRouteFromSqlite(nodeCode: String, dbPath: String): String {
        val url = "jdbc:sqlite:$dbPath"
        val props = Properties()
        
        return DriverManager.getConnection(url, props).use { conn ->
            val sql = """
                SELECT deltaVt, topo_stress_norm, canal_temperature_C, 
                       pfas_concentration_ugL, fog_route
                FROM cyboquatic_workload_telemetry ct
                JOIN canal_node cn ON cn.node_id = ct.node_id
                WHERE cn.node_code = ?
                ORDER BY ct.timestamp_utc DESC
                LIMIT 1
            """.trimIndent()
            
            conn.prepareStatement(sql).use { ps ->
                ps.setString(1, nodeCode)
                ps.executeQuery().use { rs ->
                    if (rs.next()) {
                        val deltaVt = rs.getDouble("deltaVt")
                        val topoStress = rs.getDouble("topo_stress_norm")
                        val tempC = rs.getDouble("canal_temperature_C")
                        val pfasUgL = rs.getDouble("pfas_concentration_ugL")
                        
                        // Approximate dissolved O2 and turbidity from available telemetry
                        // Higher deltaVt -> lower O2; higher topo stress -> higher turbidity
                        val approxDO2 = 8.0 - deltaVt * 5.0  // baseline 8 mg/L, decreases with workload
                        val approxTurbidity = 20.0 + topoStress * 60.0  // baseline 20 NTU
                        
                        val media = MediaSample(
                            mediumType = "water",
                            temperatureC = tempC,
                            pfasConcentrationUgL = pfasUgL,
                            dissolvedO2MgL = maxOf(0.0, approxDO2),
                            turbidityNTU = approxTurbidity
                        )
                        
                        route(media)
                    } else {
                        "FOG:NEEDS_DIAGNOSTIC"  // No data found
                    }
                }
            }
        }
    }
}

fun main(args: Array<String>) {
    var dbPath = "eco_restoration_workload.sqlite"
    var node = "PHX_CANAL_NODE_A"
    
    // Parse --db-path and --node arguments
    for (arg in args) {
        when {
            arg.startsWith("--db-path=") -> dbPath = arg.substringAfter("=")
            arg.startsWith("--node=") -> node = arg.substringAfter("=")
        }
    }
    
    println("Kotlin FOG Router Predicates")
    println("DB Path: $dbPath")
    println("Node: $node")
    
    try {
        val route = FogRouterPredicates.decideRouteFromSqlite(node, dbPath)
        println("Decided FOG route: $route")
    } catch (e: Exception) {
        System.err.println("Error deciding route: ${e.message}")
        e.printStackTrace()
    }
}
