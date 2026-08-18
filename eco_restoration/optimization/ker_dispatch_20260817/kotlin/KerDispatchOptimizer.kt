data class DispatchCandidate(
    val id: String,
    val ecoImpactValue: Double,
    val harmRisk: Double,
    val energyReqJ: Long
)

data class DispatchPlan(
    val selected: List<DispatchCandidate>,
    val totalEcoImpactValue: Double,
    val totalEnergyReqJ: Long,
    val status: String,
    val knowledgeFactor: Double,
    val harmRisk: Double
)

fun optimizeKerDispatch(
    candidates: List<DispatchCandidate>,
    harmRiskThreshold: Double,
    energyBudgetJ: Long
): DispatchPlan {
    require(harmRiskThreshold in 0.0..1.0) { "harmRiskThreshold must be in [0, 1]" }
    require(energyBudgetJ >= 0L) { "energyBudgetJ must be non-negative" }
    require(candidates.map { it.id }.toSet().size == candidates.size) { "candidate identifiers must be unique" }

    candidates.forEach {
        require(it.ecoImpactValue in 0.0..1.0) { "ecoImpactValue must be in [0, 1]" }
        require(it.harmRisk in 0.0..1.0) { "harmRisk must be in [0, 1]" }
        require(it.energyReqJ >= 0L) { "energyReqJ must be non-negative" }
    }

    val eligible = candidates
        .filter { it.harmRisk <= harmRiskThreshold && it.energyReqJ <= energyBudgetJ }
        .sortedBy { it.id }

    if (energyBudgetJ > 200_000L) {
        return DispatchPlan(
            selected = emptyList(),
            totalEcoImpactValue = 0.0,
            totalEnergyReqJ = 0L,
            status = "BUDGET_TOO_LARGE_FOR_EXACT_EDGE_SOLVER",
            knowledgeFactor = 0.50,
            harmRisk = 0.25
        )
    }

    val dp = DoubleArray((energyBudgetJ + 1L).toInt() + 1) { 0.0 }
    val chosen = Array(eligible.size) { BooleanArray((energyBudgetJ + 1L).toInt() + 1) }

    for (index in eligible.indices) {
        val candidate = eligible[index]
        val cost = candidate.energyReqJ.toInt()

        for (energy in energyBudgetJ.toInt() downTo cost) {
            val includeValue = dp[energy - cost] + candidate.ecoImpactValue
            if (includeValue > dp[energy] + 1.0e-12) {
                dp[energy] = includeValue
                chosen[index][energy] = true
            }
        }
    }

    var remainingEnergy = energyBudgetJ.toInt()
    val selected = mutableListOf<DispatchCandidate>()

    for (index in eligible.indices.reversed()) {
        val candidate = eligible[index]
        val cost = candidate.energyReqJ.toInt()

        if (remainingEnergy >= cost && chosen[index][remainingEnergy]) {
            selected += candidate
            remainingEnergy -= cost
        }
    }

    selected.reverse()
    val totalEnergy = selected.sumOf { it.energyReqJ }
    val totalImpact = selected.sumOf { it.ecoImpactValue }
    val maximumSelectedRisk = selected.maxOfOrNull { it.harmRisk } ?: 0.0
    val knowledge = if (eligible.isEmpty()) 0.40 else 0.85
    val status = if (selected.isEmpty()) "NO_ELIGIBLE_DISPATCH" else "EXACT_KNAPSACK_PLAN"

    return DispatchPlan(
        selected = selected,
        totalEcoImpactValue = totalImpact,
        totalEnergyReqJ = totalEnergy,
        status = status,
        knowledgeFactor = knowledge,
        harmRisk = maximumSelectedRisk
    )
}

fun main(args: Array<String>) {
    require(args.size >= 3 && (args.size - 2) % 4 == 0) {
        "usage: KerDispatchOptimizerKt <harm_risk_threshold> <energy_budget_J> " +
            "<id> <eco_impact_0_to_1> <harm_risk_0_to_1> <energyreqJ_integer> [...]"
    }

    val threshold = args[0].toDouble()
    val budget = args[1].toLong()
    val candidates = buildList {
        var index = 2
        while (index < args.size) {
            add(
                DispatchCandidate(
                    id = args[index],
                    ecoImpactValue = args[index + 1].toDouble(),
                    harmRisk = args[index + 2].toDouble(),
                    energyReqJ = args[index + 3].toLong()
                )
            )
            index += 4
        }
    }

    val plan = optimizeKerDispatch(candidates, threshold, budget)
    println("status=${plan.status}")
    println("total_eco_impact_value=${"%.8f".format(plan.totalEcoImpactValue)}")
    println("total_energyreqJ=${plan.totalEnergyReqJ}")
    println("knowledge_factor=${"%.8f".format(plan.knowledgeFactor)}")
    println("harm_risk=${"%.8f".format(plan.harmRisk)}")
    plan.selected.forEach {
        println(
            "selected id=${it.id} eco_impact_value=${"%.8f".format(it.ecoImpactValue)} " +
                "harm_risk=${"%.8f".format(it.harmRisk)} energyreqJ=${it.energyReqJ}"
        )
    }
}
