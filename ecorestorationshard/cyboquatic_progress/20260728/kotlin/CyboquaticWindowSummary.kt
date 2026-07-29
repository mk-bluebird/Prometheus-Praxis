// filename: ecorestorationshard/cyboquatic_progress/20260728/kotlin/CyboquaticWindowSummary.kt
// purpose: Kotlin inspector summarizing cyboquatic workload windows for AI-chat friendly views
// domain: (d) Cyboquatic workload (energyreqJ, ΔVt)
// note: non-actuating, reads from dbcyboquaticdailyprogress.sqlite

import java.sql.Connection
import java.sql.DriverManager
import java.time.LocalDate

data class WorkloadWindowSummary(
    val nodeId: String,
    val date: LocalDate,
    val domainId: String,
    val subtaskId: String,
    val energyReqJTotal: Double,
    val vtBeforeMin: Double,
    val vtAfterMax: Double,
    val deltaVtMax: Double,
    val kMetric: Double,
    val eMetric: Double,
    val rMetric: Double
)

object CyboquaticWindowSummary {

    fun computeSummary(conn: Connection, date: LocalDate, domainId: String = "d"): List<WorkloadWindowSummary> {
        val summaries = mutableMapOf<String, MutableList<Double>>()
        val vtBefores = mutableMapOf<String, MutableList<Double>>()
        val vtAfters = mutableMapOf<String, MutableList<Double>>()
        val deltas = mutableMapOf<String, MutableList<Double>>()
        val nodeSubtasks = mutableMapOf<String, String>()

        val dateStr = date.toString().replace("-", "")
        val sql = """
            SELECT node_id, subtask_id, energyreqj, vt_before, vt_after, delta_vt
            FROM dailyprogress
            WHERE domain_id = ? AND yyyymmdd = ?
            """

        conn.prepareStatement(sql).use { ps ->
            ps.setString(1, domainId)
            ps.setString(2, dateStr)
            ps.executeQuery().use { rs ->
                while (rs.next()) {
                    val nodeId = rs.getString(1)
                    val subtaskId = rs.getString(2)
                    val energyReqJ = rs.getDouble(3)
                    val vtBefore = rs.getDouble(4)
                    val vtAfter = rs.getDouble(5)
                    val deltaVt = rs.getDouble(6)

                    summaries.getOrPut(nodeId) { mutableListOf() }.add(energyReqJ)
                    vtBefores.getOrPut(nodeId) { mutableListOf() }.add(vtBefore)
                    vtAfters.getOrPut(nodeId) { mutableListOf() }.add(vtAfter)
                    deltas.getOrPut(nodeId) { mutableListOf() }.add(deltaVt)
                    nodeSubtasks[nodeId] = subtaskId
                }
            }
        }

        val out = mutableListOf<WorkloadWindowSummary>()
        for ((nodeId, energyList) in summaries) {
            val vtBeforeList = vtBefores[nodeId] ?: mutableListOf()
            val vtAfterList = vtAfters[nodeId] ?: mutableListOf()
            val deltaList = deltas[nodeId] ?: mutableListOf()

            val energyTotal = energyList.sum()
            val vtBeforeMin = vtBeforeList.minOrNull() ?: 0.0
            val vtAfterMax = vtAfterList.maxOrNull() ?: 0.0
            val deltaMax = deltaList.maxOrNull() ?: 0.0

            // Simple window-level KER-like metrics:
            // K = fraction of steps with non-increasing V_t
            // E = 1 - max(delta_vt, 0) clipped to [0,1]
            // R = max positive delta_vt clipped to [0,1]
            val nonIncreasingCount = deltaList.count { it <= 0.0 }
            val totalCount = deltaList.size.coerceAtLeast(1)
            val kMetric = nonIncreasingCount.toDouble() / totalCount.toDouble()

            val maxPositiveDelta = deltaList.filter { it > 0.0 }.maxOrNull() ?: 0.0
            val rMetric = maxPositiveDelta.coerceIn(0.0, 1.0)
            val eMetric = (1.0 - rMetric).coerceIn(0.0, 1.0)

            out.add(
                WorkloadWindowSummary(
                    nodeId = nodeId,
                    date = date,
                    domainId = domainId,
                    subtaskId = nodeSubtasks[nodeId] ?: "",
                    energyReqJTotal = energyTotal,
                    vtBeforeMin = vtBeforeMin,
                    vtAfterMax = vtAfterMax,
                    deltaVtMax = deltaMax,
                    kMetric = kMetric,
                    eMetric = eMetric,
                    rMetric = rMetric
                )
            )
        }

        return out
    }

    @JvmStatic
    fun main(args: Array<String>) {
        val dbPath = "dbcyboquaticdailyprogress.sqlite"
        val conn = DriverManager.getConnection("jdbc:sqlite:$dbPath")
        val date = LocalDate.of(2026, 7, 28)

        val summaries = computeSummary(conn, date, "d")
        for (s in summaries) {
            println(
                "node=" + s.nodeId +
                " date=" + s.date +
                " domain=" + s.domainId +
                " energy_total_J=" + s.energyReqJTotal +
                " vt_before_min=" + s.vtBeforeMin +
                " vt_after_max=" + s.vtAfterMax +
                " delta_vt_max=" + s.deltaVtMax +
                " K=" + s.kMetric +
                " E=" + s.eMetric +
                " R=" + s.rMetric
            )
        }

        conn.close()
    }
}
