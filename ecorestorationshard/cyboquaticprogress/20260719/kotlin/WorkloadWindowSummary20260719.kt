// filename: ecorestorationshard/cyboquaticprogress/20260719/kotlin/WorkloadWindowSummary20260719.kt
// destination: ecorestorationshard/cyboquaticprogress/20260719/kotlin/WorkloadWindowSummary20260719.kt
// repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
// Purpose: Kotlin utility to read dailyprogress_workload_20260719 rows and
// render a JSON-style summary for AI-chat dashboards, keeping computation
// non-actuating and focused on K,E,R and ΔVt telemetry. [file:2][file:13]

import java.sql.Connection
import java.sql.DriverManager
import java.sql.ResultSet

data class WorkloadSummary(
    val nodeId: String,
    val energyReqJ: Double,
    val deltaEnergyJ: Double,
    val vtPrev: Double,
    val vtCurr: Double,
    val deltaVt: Double,
    val k: Double,
    val e: Double,
    val r: Double,
    val evidenceHex: String,
    val hexLogicalName: String
)

fun main() {
    val dbPath = "ecorestorationshard/db/dbcyboquaticdailyprogress.sqlite"
    val conn: Connection = DriverManager.getConnection("jdbc:sqlite:$dbPath")
    val sql = """
        SELECT node_id, energyreq_j, delta_energy_j,
               vt_prev, vt_curr, delta_vt,
               k_factor, e_factor, r_factor,
               evidence_hex, hex_logical_name
        FROM dailyprogress_workload_20260719
        WHERE workday_yyyymmdd = '20260719'
    """.trimIndent()

    conn.use { c ->
        c.createStatement().use { st ->
            val rs: ResultSet = st.executeQuery(sql)
            while (rs.next()) {
                val summary = WorkloadSummary(
                    nodeId        = rs.getString(1),
                    energyReqJ    = rs.getDouble(2),
                    deltaEnergyJ  = rs.getDouble(3),
                    vtPrev        = rs.getDouble(4),
                    vtCurr        = rs.getDouble(5),
                    deltaVt       = rs.getDouble(6),
                    k             = rs.getDouble(7),
                    e             = rs.getDouble(8),
                    r             = rs.getDouble(9),
                    evidenceHex   = rs.getString(10),
                    hexLogicalName= rs.getString(11)
                )
                println("""{
  "nodeId": "${summary.nodeId}",
  "energyReqJ": ${summary.energyReqJ},
  "deltaEnergyJ": ${summary.deltaEnergyJ},
  "vtPrev": ${summary.vtPrev},
  "vtCurr": ${summary.vtCurr},
  "deltaVt": ${summary.deltaVt},
  "K": ${summary.k},
  "E": ${summary.e},
  "R": ${summary.r},
  "evidenceHex": "${summary.evidenceHex}",
  "hexLogicalName": "${summary.hexLogicalName}"
}""")
            }
        }
    }
}
