// filename: ecorestorationshard/cyboquaticprogress/20260727/kotlin/CyboquaticWorkloadSummary.kt
// purpose: Kotlin summary utility for daily cyboquatic workload slices, AI-chat friendly JSON
// domain: (d) Cyboquatic workload energetics
// anchor: PHXWORKLOADENERGYDV20260727 / 0x20260727PHX3345NWorkloadEnergyDeltaVt

import java.sql.Connection
import java.sql.DriverManager
import java.sql.ResultSet
import java.time.LocalDate

data class WorkloadSummary(
    val yyyymmdd: String,
    val domain: String,
    val subtaskId: String,
    val count: Int,
    val avgEnergyReqJ: Double,
    val avgDeltaVt: Double,
    val avgREnergy: Double,
    val avgRHydraulics: Double,
    val avgRUncertainty: Double,
    val evidenceHex: String
)

object CyboquaticWorkloadSummary {

    private fun open(path: String): Connection {
        val url = "jdbc:sqlite:$path"
        return DriverManager.getConnection(url)
    }

    private fun querySummary(conn: Connection, yyyymmdd: String): WorkloadSummary? {
        val sql = """
            SELECT
                COUNT(*) AS cnt,
                AVG(energy_req_j) AS avg_energy_req_j,
                AVG(delta_vt) AS avg_delta_vt,
                AVG(r_energy) AS avg_r_energy,
                AVG(r_hydraulics) AS avg_r_hydraulics,
                AVG(r_uncertainty) AS avg_r_uncertainty,
                MAX(evidencehex) AS evidence_hex,
                MAX(domain) AS domain,
                MAX(subtaskid) AS subtask_id
            FROM dailyprogress
            WHERE yyyymmdd = ?
              AND domain = 'CYBOQUATIC'
              AND subtaskid LIKE 'PHX-CANAL-WORKLOAD-ENERGYDV-%'
        """.trimIndent()

        conn.prepareStatement(sql).use { ps ->
            ps.setString(1, yyyymmdd)
            val rs: ResultSet = ps.executeQuery()
            if (rs.next()) {
                val cnt = rs.getInt("cnt")
                if (cnt == 0) {
                    return null
                }
                return WorkloadSummary(
                    yyyymmdd = yyyymmdd,
                    domain = rs.getString("domain"),
                    subtaskId = rs.getString("subtask_id"),
                    count = cnt,
                    avgEnergyReqJ = rs.getDouble("avg_energy_req_j"),
                    avgDeltaVt = rs.getDouble("avg_delta_vt"),
                    avgREnergy = rs.getDouble("avg_r_energy"),
                    avgRHydraulics = rs.getDouble("avg_r_hydraulics"),
                    avgRUncertainty = rs.getDouble("avg_r_uncertainty"),
                    evidenceHex = rs.getString("evidence_hex")
                )
            }
        }
        return null
    }

    fun main(args: Array<String>) {
        if (args.size < 2) {
            System.err.println("Usage: CyboquaticWorkloadSummary db_path yyyymmdd")
            return
        }
        val dbPath = args[0]
        val yyyymmdd = args[1]
        val conn = open(dbPath)
        conn.use {
            val summary = querySummary(it, yyyymmdd)
            if (summary == null) {
                println("""{"yyyymmdd":"$yyyymmdd","domain":"CYBOQUATIC","subtaskId":"PHX-CANAL-WORKLOAD-ENERGYDV-20260727","count":0}""")
            } else {
                val json = """
                {
                  "yyyymmdd": "${summary.yyyymmdd}",
                  "domain": "${summary.domain}",
                  "subtaskId": "${summary.subtaskId}",
                  "count": ${summary.count},
                  "avgEnergyReqJ": ${summary.avgEnergyReqJ},
                  "avgDeltaVt": ${summary.avgDeltaVt},
                  "avgREnergy": ${summary.avgREnergy},
                  "avgRHydraulics": ${summary.avgRHydraulics},
                  "avgRUncertainty": ${summary.avgRUncertainty},
                  "evidenceHex": "${summary.evidenceHex}"
                }
                """.trimIndent()
                println(json)
            }
        }
    }
}
