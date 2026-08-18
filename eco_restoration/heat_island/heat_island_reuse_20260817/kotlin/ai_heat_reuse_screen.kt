import kotlin.math.max
import kotlin.math.min

data class HeatSink(
    val sinkType: String,
    val heatDemandJ: Double,
    val currentTemperatureC: Double,
    val maximumTemperatureC: Double,
    val habitatSensitivity: Double
)

data class HeatReuseResult(
    val availableFacilityEnergyJ: Double,
    val recoverableHeatJ: Double,
    val allocatedHeatJ: Double,
    val status: String,
    val knowledgeFactor: Double,
    val ecoImpactValue: Double,
    val harmRisk: Double
)

fun clamp01(value: Double): Double = max(0.0, min(1.0, value))

fun screenHeatReuse(
    itPowerW: Double,
    pue: Double,
    durationSeconds: Double,
    recoverableHeatFraction: Double,
    sink: HeatSink
): HeatReuseResult {
    require(itPowerW >= 0.0 && itPowerW.isFinite()) { "itPowerW must be finite and non-negative" }
    require(pue >= 1.0 && pue.isFinite()) { "PUE must be finite and at least 1" }
    require(durationSeconds >= 0.0 && durationSeconds.isFinite()) { "durationSeconds must be finite and non-negative" }
    require(recoverableHeatFraction in 0.0..1.0) { "recoverableHeatFraction must be in [0, 1]" }
    require(sink.heatDemandJ >= 0.0 && sink.currentTemperatureC.isFinite() && sink.maximumTemperatureC.isFinite()) {
        "heat sink data are invalid"
    }
    require(sink.habitatSensitivity in 0.0..1.0) { "habitatSensitivity must be in [0, 1]" }

    val availableFacilityEnergyJ = itPowerW * pue * durationSeconds
    val recoverableHeatJ = availableFacilityEnergyJ * recoverableHeatFraction

    if (sink.sinkType == "CANAL_WATER") {
        return HeatReuseResult(
            availableFacilityEnergyJ,
            recoverableHeatJ,
            0.0,
            "REJECT_CANAL_WATER_WARMING",
            0.35,
            0.05,
            0.90
        )
    }

    if (sink.sinkType !in setOf("GREENHOUSE", "SEED_GERMINATION", "COMPOST")) {
        return HeatReuseResult(
            availableFacilityEnergyJ,
            recoverableHeatJ,
            0.0,
            "REJECT_UNKNOWN_HEAT_SINK",
            0.20,
            0.00,
            0.80
        )
    }

    if (sink.currentTemperatureC >= sink.maximumTemperatureC) {
        return HeatReuseResult(
            availableFacilityEnergyJ,
            recoverableHeatJ,
            0.0,
            "HOLD_TEMPERATURE_LIMIT",
            0.60,
            0.15,
            0.65
        )
    }

    val allocation = min(recoverableHeatJ, sink.heatDemandJ)
    val demandCoverage = if (sink.heatDemandJ <= 0.0) 0.0 else allocation / sink.heatDemandJ
    val risk = clamp01(0.25 + 0.30 * sink.habitatSensitivity)
    val knowledge = clamp01(0.70 - 0.20 * sink.habitatSensitivity)
    val impact = clamp01(knowledge * (1.0 - risk) * demandCoverage)

    return HeatReuseResult(
        availableFacilityEnergyJ,
        recoverableHeatJ,
        allocation,
        "ELIGIBLE_FOR_SITE_THERMAL_REVIEW",
        knowledge,
        impact,
        risk
    )
}

fun main(args: Array<String>) {
    require(args.size == 9) {
        "usage: AiHeatReuseScreenKt <P_IT_W> <PUE> <duration_s> <recoverable_heat_fraction_0_to_1> " +
            "<GREENHOUSE|SEED_GERMINATION|COMPOST|CANAL_WATER> <sink_demand_J> " +
            "<current_temp_C> <maximum_temp_C> <habitat_sensitivity_0_to_1>"
    }

    val result = screenHeatReuse(
        itPowerW = args[0].toDouble(),
        pue = args[1].toDouble(),
        durationSeconds = args[2].toDouble(),
        recoverableHeatFraction = args[3].toDouble(),
        sink = HeatSink(
            sinkType = args[4],
            heatDemandJ = args[5].toDouble(),
            currentTemperatureC = args[6].toDouble(),
            maximumTemperatureC = args[7].toDouble(),
            habitatSensitivity = args[8].toDouble()
        )
    )

    println("facility_energy_J=${"%.8f".format(result.availableFacilityEnergyJ)}")
    println("recoverable_heat_J=${"%.8f".format(result.recoverableHeatJ)}")
    println("allocated_heat_J=${"%.8f".format(result.allocatedHeatJ)}")
    println("status=${result.status}")
    println("knowledge_factor=${"%.8f".format(result.knowledgeFactor)}")
    println("eco_impact_value=${"%.8f".format(result.ecoImpactValue)}")
    println("harm_risk=${"%.8f".format(result.harmRisk)}")
}
