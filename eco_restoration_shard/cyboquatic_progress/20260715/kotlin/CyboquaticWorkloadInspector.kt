// filename: eco_restoration_shard/cyboquatic_progress/20260715/kotlin/CyboquaticWorkloadInspector.kt
// purpose: Kotlin non-actuating inspector that summarizes energyreqJ and ΔVt windows
//          from the daily SQLite DB, for AI-chat and dashboard usage.

package org.cyboquatic

import java.sql.Connection
import java.sql.DriverManager
import java.sql.ResultSet

data class WorkloadSummary(
    val yyyymmdd: String,
    val segmentId: String,
    val avgEnergyReqJ: Double,
    val avgDeltaVt: Double,
    val kAvg: Double,
    val eAvg: Double,
    val rAvg: Double
)

object CyboquaticWorkloadInspector {

    private fun getConnection(dbPath: String): Connection {
        val url = "jdbc:sqlite:$dbPath"
        return DriverManager.getConnection(url)
    }

    fun summarizeDay(dbPath: String, yyyymmdd: String): List<WorkloadSummary> {
        val conn = getConnection(dbPath)
        conn.use { c ->
            val sql = """
                SELECT segment_id,
                       AVG(energyreq_j) AS avg_energyreq_j,
                       AVG(deltavt)     AS avg_deltavt,
                       AVG(k_factor)    AS k_avg,
                       AVG(e_factor)    AS e_avg,
                       AVG(r_factor)    AS r_avg
                FROM daily_progress
                WHERE yyyymmdd = ?
                  AND domain = 'cyboquatic_workload'
                GROUP BY segment_id
            """.trimIndent()
            val summaries = mutableListOf<WorkloadSummary>()
            c.prepareStatement(sql).use { ps ->
                ps.setString(1, yyyymmdd)
                val rs: ResultSet = ps.executeQuery()
                while (rs.next()) {
                    summaries.add(
                        WorkloadSummary(
                            yyyymmdd = yyyymmdd,
                            segmentId = rs.getString("segment_id"),
                            avgEnergyReqJ = rs.getDouble("avg_energyreq_j"),
                            avgDeltaVt = rs.getDouble("avg_deltavt"),
                            kAvg = rs.getDouble("k_avg"),
                            eAvg = rs.getDouble("e_avg"),
                            rAvg = rs.getDouble("r_avg")
                        )
                    )
                }
            }
            return summaries
        }
    }

    @JvmStatic
    fun main(args: Array<String>) {
        val dbPath = "cyboquatic_daily_progress.sqlite"
        val day = "20260715"
        val summaries = summarizeDay(dbPath, day)
        for (s in summaries) {
            println("Segment: ${s.segmentId} @ $day")
            println("  avgEnergyReqJ = ${s.avgEnergyReqJ}")
            println("  avgDeltaVt    = ${s.avgDeltaVt}")
            println("  kAvg          = ${s.kAvg}")
            println("  eAvg          = ${s.eAvg}")
            println("  rAvg          = ${s.rAvg}")
            println("------------------------------")
        }
    }
}
