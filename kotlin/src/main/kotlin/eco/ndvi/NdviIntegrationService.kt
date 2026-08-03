// File: kotlin/src/main/kotlin/eco/ndvi/NdviIntegrationService.kt
package eco.ndvi

import java.sql.Connection
import java.sql.DriverManager
import java.time.LocalDate
import kotlin.math.exp

/**
 * Kotlin NDVI Integration Service
 *
 * - Fetches Sentinel-2 NDVI (here simulated) every 5 days.
 * - Computes ndvi_coeff per hex with a Lyapunov-Razumikhin-inspired bound:
 *   ndvi_coeff is constrained to keep a proxy Lyapunov function V_ndvi within a corridor.
 * - Updates hex_ndvi_profile via SQLite.
 *
 * This shard is wired to the Prometheus-Praxis SQLite database directly.
 */

data class HexNdviSample(
    val hexId: String,
    val ndviMean: Double,
    val ndviVar: Double
)

class NdviIntegrationService(
    private val sqlitePath: String = "prometheus_praxis.db"
) {

    private fun connect(): Connection =
        DriverManager.getConnection("jdbc:sqlite:$sqlitePath")

    // Simulated Sentinel-2 NDVI fetch. In a real service, this would call an API.
    private fun fetchNdviSamples(date: LocalDate): List<HexNdviSample> {
        // Example: three hexes with different vegetation signals.
        return listOf(
            HexNdviSample("hex_PHX_001", 0.35, 0.02),
            HexNdviSample("hex_PHX_002", 0.28, 0.03),
            HexNdviSample("hex_PHX_003", 0.42, 0.015)
        )
    }

    /**
     * Compute ndvi_coeff from mean/variance using a Razumikhin-like bound:
     *
     * Let V_ndvi = a * (ndviMean - ndviTarget)^2 + b * ndviVar
     * Choose ndvi_coeff = exp(-V_ndvi) so high vegetation and low variance
     * drive V_ndvi down and ndvi_coeff up, while still bounded in [0,1].
     */
    private fun computeNdviCoeff(sample: HexNdviSample,
                                 ndviTarget: Double = 0.35,
                                 a: Double = 5.0,
                                 b: Double = 1.0): Double {
        val x = sample.ndviMean - ndviTarget
        val V = a * x * x + b * sample.ndviVar
        val coeff = exp(-V)
        return coeff.coerceIn(0.0, 1.0)
    }

    private fun updateHexNdviProfile(conn: Connection,
                                     date: LocalDate,
                                     sample: HexNdviSample,
                                     ndviCoeff: Double) {
        val sql = """
            INSERT INTO hex_ndvi_profile (hex_id, ts, ndvi_mean, ndvi_var, ndvi_coeff)
            VALUES (?, ?, ?, ?, ?)
        """.trimIndent()
        conn.prepareStatement(sql).use { ps ->
            ps.setString(1, sample.hexId)
            ps.setString(2, date.toString())
            ps.setDouble(3, sample.ndviMean)
            ps.setDouble(4, sample.ndviVar)
            ps.setDouble(5, ndviCoeff)
            ps.executeUpdate()
        }
    }

    fun runOnce(date: LocalDate = LocalDate.now()) {
        val samples = fetchNdviSamples(date)
        connect().use { conn ->
            conn.autoCommit = false
            for (s in samples) {
                val coeff = computeNdviCoeff(s)
                updateHexNdviProfile(conn, date, s, coeff)
            }
            conn.commit()
        }
    }
}

// Example entry point for a 5-day scheduler.
fun main() {
    val service = NdviIntegrationService()
    val today = LocalDate.now()
    service.runOnce(today)
    println("NDVI integration run completed for $today")
}
