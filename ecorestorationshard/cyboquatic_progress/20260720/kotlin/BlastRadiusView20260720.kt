// filename: ecorestorationshard/cyboquatic_progress/20260720/kotlin/BlastRadiusView20260720.kt
// destination: ecorestorationshard/cyboquatic_progress/20260720/kotlin/BlastRadiusView20260720.kt
// domain: g (blast-radius surcharge envelopes).[file:2]

import java.sql.Connection
import java.sql.DriverManager
import java.sql.ResultSet

data class BlastRadiusRow(
    val canalSegmentId: String,
    val surchargeLevelM: Double,
    val breachProb: Double,
    val radiusM: Double,
    val impactClass: String,
    val kMetric: Double,
    val eMetric: Double,
    val rMetric: Double,
    val vtResidual: Double
)

object BlastRadiusView20260720 {

    private const val DB_PATH =
        "jdbc:sqlite:ecorestorationshard/db/dbcyboquaticdailyprogress.sqlite" // shared DB.[file:2]

    private fun open(): Connection =
        DriverManager.getConnection(DB_PATH)

    fun listForDay(yyyymmdd: String): List<BlastRadiusRow> {
        val sql = """
            SELECT 
                br.canal_segment_id,
                br.surcharge_level_m,
                br.breach_prob,
                br.radius_m,
                br.impact_class,
                dp.k_metric,
                dp.e_metric,
                dp.r_metric,
                dp.vt_residual
            FROM blastradius_surcharge br
            JOIN dailyprogress dp
              ON dp.yyyymmdd = br.yyyymmdd
             AND dp.canal_segment_id = br.canal_segment_id
            WHERE br.yyyymmdd = ?
        """.trimIndent()

        open().use { conn ->
            conn.prepareStatement(sql).use { ps ->
                ps.setString(1, yyyymmdd)
                val rs: ResultSet = ps.executeQuery()
                val rows = mutableListOf<BlastRadiusRow>()
                while (rs.next()) {
                    rows += BlastRadiusRow(
                        canalSegmentId = rs.getString(1),
                        surchargeLevelM = rs.getDouble(2),
                        breachProb = rs.getDouble(3),
                        radiusM = rs.getDouble(4),
                        impactClass = rs.getString(5),
                        kMetric = rs.getDouble(6),
                        eMetric = rs.getDouble(7),
                        rMetric = rs.getDouble(8),
                        vtResidual = rs.getDouble(9)
                    )
                }
                return rows
            }
        }
    }
}

fun main() {
    val rows = BlastRadiusView20260720.listForDay("20260720")
    rows.forEach { println(it) }
}
