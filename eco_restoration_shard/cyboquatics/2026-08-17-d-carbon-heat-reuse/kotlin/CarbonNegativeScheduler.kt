import kotlin.math.max

data class Workload(
    val id: String,
    val energyReqJ: Double,
    val sequesteredKgCo2e: Double,
    val emittedKgCo2e: Double
) {
    val netCarbonKgCo2e: Double
        get() = sequesteredKgCo2e - emittedKgCo2e

    val netCarbonPerJoule: Double
        get() = if (energyReqJ <= 0.0) Double.NEGATIVE_INFINITY else netCarbonKgCo2e / energyReqJ
}

data class ScheduleResult(
    val selected: List<Workload>,
    val deferred: List<Workload>,
    val totalEnergyJ: Double,
    val totalNetCarbonKgCo2e: Double,
    val knowledgeFactor: Double,
    val ecoImpactValue: Double,
    val harmRisk: Double
)

fun scheduleCarbonNegativeWorkloads(
    workloads: List<Workload>,
    energyBudgetJ: Double
): ScheduleResult {
    require(energyBudgetJ >= 0.0 && energyBudgetJ.isFinite()) { "energy budget must be finite and non-negative" }
    require(workloads.map { it.id }.toSet().size == workloads.size) { "workload identifiers must be unique" }

    workloads.forEach {
        require(it.energyReqJ > 0.0 && it.energyReqJ.isFinite()) { "energy requirement must be finite and positive" }
        require(it.sequesteredKgCo2e >= 0.0 && it.sequesteredKgCo2e.isFinite()) { "sequestered carbon must be finite and non-negative" }
        require(it.emittedKgCo2e >= 0.0 && it.emittedKgCo2e.isFinite()) { "emitted carbon must be finite and non-negative" }
    }

    val ordered = workloads.sortedWith(
        compareByDescending<Workload> { it.netCarbonPerJoule }
            .thenByDescending { it.netCarbonKgCo2e }
            .thenBy { it.id }
    )

    var remainingEnergy = energyBudgetJ
    val selected = mutableListOf<Workload>()
    val deferred = mutableListOf<Workload>()

    for (workload in ordered) {
        if (workload.netCarbonKgCo2e > 0.0 && workload.energyReqJ <= remainingEnergy) {
            selected += workload
            remainingEnergy -= workload.energyReqJ
        } else {
            deferred += workload
        }
    }

    val totalEnergy = selected.sumOf { it.energyReqJ }
    val totalNetCarbon = selected.sumOf { it.netCarbonKgCo2e }
    val coverage = if (energyBudgetJ == 0.0) 0.0 else totalEnergy / energyBudgetJ
    val knowledgeFactor = max(0.0, 0.80 - 0.20 * deferred.count { it.netCarbonKgCo2e > 0.0 }.toDouble() / max(1, workloads.size))
    val harmRisk = max(0.0, 0.25 - 0.10 * coverage)
    val ecoImpactValue = max(0.0, minOf(1.0, totalNetCarbon / max(1.0, totalNetCarbon + 1.0)))

    return ScheduleResult(
        selected,
        deferred,
        totalEnergy,
        totalNetCarbon,
        knowledgeFactor,
        ecoImpactValue,
        harmRisk
    )
}

fun main(args: Array<String>) {
    require(args.size >= 5 && (args.size - 1) % 4 == 0) {
        "usage: CarbonNegativeSchedulerKt <energy_budget_J> " +
            "<id> <energyreqJ> <sequestered_kg_co2e> <emitted_kg_co2e> [...]"
    }

    val budget = args[0].toDouble()
    val workloads = buildList {
        var index = 1
        while (index < args.size) {
            add(
                Workload(
                    id = args[index],
                    energyReqJ = args[index + 1].toDouble(),
                    sequesteredKgCo2e = args[index + 2].toDouble(),
                    emittedKgCo2e = args[index + 3].toDouble()
                )
            )
            index += 4
        }
    }

    val result = scheduleCarbonNegativeWorkloads(workloads, budget)
    result.selected.forEach {
        println(
            "selected id=${it.id} energyreqJ=${"%.6f".format(it.energyReqJ)} " +
                "net_carbon_kg_co2e=${"%.6f".format(it.netCarbonKgCo2e)} " +
                "net_carbon_per_joule=${"%.12f".format(it.netCarbonPerJoule)}"
        )
    }
    result.deferred.forEach {
        println(
            "deferred id=${it.id} energyreqJ=${"%.6f".format(it.energyReqJ)} " +
                "net_carbon_kg_co2e=${"%.6f".format(it.netCarbonKgCo2e)}"
        )
    }
    println("total_energy_J=${"%.6f".format(result.totalEnergyJ)}")
    println("total_net_carbon_kg_co2e=${"%.6f".format(result.totalNetCarbonKgCo2e)}")
    println("knowledge_factor=${"%.6f".format(result.knowledgeFactor)}")
    println("eco_impact_value=${"%.6f".format(result.ecoImpactValue)}")
    println("harm_risk=${"%.6f".format(result.harmRisk)}")
}
