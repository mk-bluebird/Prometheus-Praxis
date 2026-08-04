// File: kotlin/src/main/kotlin/org/cyboquatic/blast/BlastRadiusPredictor.kt
package org.cyboquatic.blast

import java.sql.Connection
import java.sql.DriverManager
import java.sql.PreparedStatement
import java.sql.ResultSet

/**
 * Kotlin micro-service that runs a parameterised query joining real-time
 * sensor readings with pre-computed blast-radius tables to return the
 * predicted flood polygon within ~100 ms.
 *
 * Assumes the C++ RTU has created an in-memory SQLite database with
 * tables:
 *  - realtime_sensor_readings (timestamp_ms, canal_id, water_level_m, pressure_kPa)
 *  - blast_radius_table (radius_id, canal_id, level_min_m, level_max_m,
 *                        pressure_min_kPa, pressure_max_kPa, flood_polygon_id)
 *
 * The micro-service connects via SQLite shared-cache URI.
 */
object BlastRadiusPredictor {

    private const val JDBC_URL = "jdbc:sqlite:file:blast_radius_db?mode=memory&cache=shared"

    fun predictFloodPolygon(canalId: String): String? {
        val startNs = System.nanoTime()
        DriverManager.getConnection(JDBC_URL).use { conn ->
            conn.autoCommit = false

            // Step 1: get latest sensor reading for the canal.
            val latest = getLatestReading(conn, canalId) ?: return null

            // Step 2: join with blast_radius_table to find matching polygon.
            val polygonId = queryBlastRadius(conn, canalId, latest.waterLevel, latest.pressure)

            conn.commit()
            val elapsedMs = (System.nanoTime() - startNs) / 1_000_000.0
            // For monitoring, one could log elapsedMs and ensure < 100 ms.
            return polygonId
        }
    }

    private data class Reading(val waterLevel: Double, val pressure: Double)

    private fun getLatestReading(conn: Connection, canalId: String): Reading? {
        val sql = """
            SELECT water_level_m, pressure_kPa
            FROM realtime_sensor_readings
            WHERE canal_id = ?
            ORDER BY timestamp_ms DESC
            LIMIT 1
        """.trimIndent()
        conn.prepareStatement(sql).use { ps ->
            ps.setString(1, canalId)
            ps.executeQuery().use { rs ->
                return if (rs.next()) {
                    Reading(
                        waterLevel = rs.getDouble("water_level_m"),
                        pressure = rs.getDouble("pressure_kPa")
                    )
                } else null
            }
        }
    }

    private fun queryBlastRadius(conn: Connection, canalId: String, waterLevel: Double, pressure: Double): String? {
        val sql = """
            SELECT flood_polygon_id
            FROM blast_radius_table
            WHERE canal_id = ?
              AND ? BETWEEN level_min_m AND level_max_m
              AND ? BETWEEN pressure_min_kPa AND pressure_max_kPa
            ORDER BY radius_id ASC
            LIMIT 1
        """.trimIndent()
        conn.prepareStatement(sql).use { ps ->
            ps.setString(1, canalId)
            ps.setDouble(2, waterLevel)
            ps.setDouble(3, pressure)
            ps.executeQuery().use { rs ->
                return if (rs.next()) rs.getString("flood_polygon_id") else null
            }
        }
    }
}

fun main() {
    val polygonId = BlastRadiusPredictor.predictFloodPolygon("canal-01")
    println("Predicted flood polygon: ${polygonId ?: "none"}")
}
