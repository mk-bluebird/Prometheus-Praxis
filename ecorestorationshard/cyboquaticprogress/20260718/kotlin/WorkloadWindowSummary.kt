// filename: ecorestorationshard/cyboquaticprogress/20260718/kotlin/WorkloadWindowSummary.kt
// purpose: Kotlin summary utility for AI-chat and dashboards, aggregating K,E,R and energyreqJ per node.
// repo-target: https://github.com/mk-bluebird/Prometheus-Praxis

import java.sql.Connection
import java.sql.DriverManager
import java.sql.ResultSet

data class WorkloadWindowSummary(
    val nodeId: String,
    val totalEnergyJ: Double,
    val avgDeltaVt: Double,
    val avgK: Double,
    val avgE: Double,
    val avgR: Double,
    val windows: Int
)

object WorkloadSummary {

    private const val DB_PATH =
        "ecorestorationshard/cyboquaticprogress/db_cyboquatic_daily_progress.sqlite"

    private fun connect(): Connection {
        val url = "jdbc:sqlite:$DB_PATH"
        return DriverManager.getConnection(url)
    }

    fun summarizeDay(yyyymmdd: String, domainId: String): List<WorkloadWindowSummary> {
        val sql = """
            SELECT nodeid,
                   SUM(energyreqJ) AS energyJ,
                   AVG(deltaVt) AS avgDeltaVt,
                   AVG(kscore) AS avgK,
                   AVG(escore) AS avgE,
                   AVG(rscore) AS avgR,
                   COUNT(*) AS windows
            FROM dailyprogress
            WHERE yyyymmdd = ?
              AND domain = ?
            GROUP BY nodeid
        """.trimIndent()

        val summaries = mutableListOf<WorkloadWindowSummary>()
        connect().use { conn ->
            conn.prepareStatement(sql).use { ps ->
                ps.setString(1, yyyymmdd)
                ps.setString(2, domainId)
                val rs: ResultSet = ps.executeQuery()
                while (rs.next()) {
                    val nodeId = rs.getString("nodeid")
                    val energyJ = rs.getDouble("energyJ")
                    val avgDeltaVt = rs.getDouble("avgDeltaVt")
                    val avgK = rs.getDouble("avgK")
                    val avgE = rs.getDouble("avgE")
                    val avgR = rs.getDouble("avgR")
                    val windows = rs.getInt("windows")
                    summaries.add(
                        WorkloadWindowSummary(
                            nodeId = nodeId,
                            totalEnergyJ = energyJ,
                            avgDeltaVt = avgDeltaVt,
                            avgK = avgK,
                            avgE = avgE,
                            avgR = avgR,
                            windows = windows
                        )
                    )
                }
            }
        }
        return summaries
    }

    @JvmStatic
    fun main(args: Array<String>) {
        val yyyymmdd = "20260718"
        val domainId = "WORKLOADENERGYDV"
        val summaries = summarizeDay(yyyymmdd, domainId)
        for (s in summaries) {
            println(
                "node=${s.nodeId} energyJ=${s.totalEnergyJ} " +
                    "avgDeltaVt=${s.avgDeltaVt} K=${s.avgK} E=${s.avgE} R=${s.avgR} windows=${s.windows}"
            )
        }
    }
}
