// File: kotlin/eco_audit/CusumOptimalStopping.kt

data class CusumParams(
    val kThreshold: Double,   // reference net carbon flux per sample
    val meanFlux: Double,     // mean net carbon flux under drift
    val varFlux: Double       // variance of net carbon flux
)

data class CusumState(
    val sampleId: Long,
    val sValue: Double
)

class CusumOptimalStopping(
    private val sqlClient: SqlClient
) {
    /**
     * Compute an approximate optimal CUSUM threshold h_cusum based on sequential analysis
     * for detecting a shift in mean net carbon flux.
     *
     * For a shift from meanFlux0 to meanFlux1 (carbon-negative to drifting),
     * the drift per step in CUSUM is Δμ = meanFlux1 - kThreshold.
     * Assuming Gaussian increments with variance varFlux, a simple approximation:
     *
     *   h_cusum ≈ (varFlux / Δμ) * log( (1 - α) / β )
     *
     * where α is false-alarm probability and β is miss probability.
     */
    fun computeOptimalThreshold(nodeId: String,
                                falseAlarmProb: Double,
                                missProb: Double): Double {
        val params = loadCusumParams(nodeId)
        val deltaMu = params.meanFlux - params.kThreshold
        if (deltaMu <= 0.0) return defaultThreshold()

        val logTerm = kotlin.math.ln((1.0 - falseAlarmProb) / missProb)
        val h = (params.varFlux / deltaMu) * logTerm
        return h.coerceAtLeast(minThreshold())
    }

    fun updateThresholdInSql(nodeId: String,
                             falseAlarmProb: Double,
                             missProb: Double) {
        val h = computeOptimalThreshold(nodeId, falseAlarmProb, missProb)
        sqlClient.execute(
            """
            INSERT INTO cusum_config (node_id, h_cusum, updated_utc)
            VALUES (?, ?, strftime('%s','now'))
            """.trimIndent(),
            nodeId, h
        )
    }

    private fun loadCusumParams(nodeId: String): CusumParams {
        val row = sqlClient.queryOne(
            """
            SELECT k_threshold, mean_flux, var_flux
            FROM ker_cusum_params
            WHERE node_id = ?
            ORDER BY updated_utc DESC
            LIMIT 1
            """.trimIndent(), nodeId
        )
        return CusumParams(
            kThreshold = row.getDouble("k_threshold"),
            meanFlux   = row.getDouble("mean_flux"),
            varFlux    = row.getDouble("var_flux")
        )
    }

    private fun defaultThreshold() = 1.0
    private fun minThreshold() = 0.1
}
