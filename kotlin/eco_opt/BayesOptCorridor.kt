// File: kotlin/eco_opt/BayesOptCorridor.kt

data class AerationSetting(val segmentId: String, val rate: Double)
data class GreenFractionSetting(val h3Index: String, val target: Double)

data class BayesOptState(
    val iteration: Int,
    val aerationSettings: List<AerationSetting>,
    val greenSettings: List<GreenFractionSetting>,
    val utility: Double
)

class CorridorBayesOptimizer(
    private val sqlClient: SqlClient,
    private val cplusplusMpc: MpcClient,
    private val luaMultigrid: LuaMultigridClient
) {
    /**
     * Multi-objective Bayesian optimisation loop:
     * maximise utility = w_pfas * PFAS removal + w_lst * LST reduction.
     */
    suspend fun runIteration(corridorId: Long, iteration: Int) {
        val currentState = loadLastState(corridorId)
        val proposal = proposeNextSettings(currentState)

        // Apply proposed aeration and green-fraction settings
        cplusplusMpc.updateAerationRates(proposal.aerationSettings)
        luaMultigrid.updateGreenTargets(proposal.greenSettings)

        // Wait for system to evolve, then query performance from SQL views
        val perf = queryCorridorPerformance(corridorId)

        val utility = computeUtility(perf)
        val newState = BayesOptState(iteration, proposal.aerationSettings, proposal.greenSettings, utility)

        // Store in bayes_opt_state table
        persistState(corridorId, newState)
    }

    private fun loadLastState(corridorId: Long): BayesOptState? {
        val row = sqlClient.queryOptional(
            """
            SELECT iteration, aeration_blob, green_blob, utility
            FROM bayes_opt_state
            WHERE corridor_id = ?
            ORDER BY iteration DESC
            LIMIT 1
            """.trimIndent(), corridorId
        ) ?: return null

        return deserializeState(row)
    }

    private fun proposeNextSettings(current: BayesOptState?): BayesOptState {
        // Simple Gaussian process surrogate / random exploration placeholder
        // In production, call a GP library; here we lightly perturb current settings.
        val aeration = current?.aerationSettings?.map {
            it.copy(rate = (it.rate * 1.05).coerceIn(minAeration(), maxAeration()))
        } ?: defaultAerationSettings()

        val green = current?.greenSettings?.map {
            it.copy(target = (it.target * 1.05).coerceIn(0.0, 1.0))
        } ?: defaultGreenSettings()

        return BayesOptState((current?.iteration ?: 0) + 1, aeration, green, utility = 0.0)
    }

    private fun queryCorridorPerformance(corridorId: Long): CorridorPerformance {
        val row = sqlClient.queryOne(
            """
            SELECT avg_pfas_removal, avg_lst_drop_c
            FROM v_corridor_perf
            WHERE corridor_id = ?
            """.trimIndent(), corridorId
        )
        return CorridorPerformance(
            avgPfasRemoval = row.getDouble("avg_pfas_removal"),
            avgLstDropC    = row.getDouble("avg_lst_drop_c")
        )
    }

    private fun computeUtility(perf: CorridorPerformance): Double {
        val wPfas = 0.6
        val wLst  = 0.4
        return wPfas * perf.avgPfasRemoval + wLst * perf.avgLstDropC
    }

    private fun persistState(corridorId: Long, state: BayesOptState) {
        val aerBlob = serializeAeration(state.aerationSettings)
        val greenBlob = serializeGreen(state.greenSettings)
        sqlClient.execute(
            """
            INSERT INTO bayes_opt_state (corridor_id, iteration, aeration_blob, green_blob, utility, updated_utc)
            VALUES (?, ?, ?, ?, ?, strftime('%s','now'))
            """.trimIndent(),
            corridorId, state.iteration, aerBlob, greenBlob, state.utility
        )
    }

    private fun minAeration() = 0.0
    private fun maxAeration() = 1.0

    // ... serialization/deserialization helpers omitted for brevity ...
}
