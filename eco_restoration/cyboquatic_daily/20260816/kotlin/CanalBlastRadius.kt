import kotlin.math.sqrt

data class Assessment(
    val baseRadiusM: Double,
    val conservativeRadiusM: Double,
    val knowledgeFactor: Double,
    val ecoImpactValue: Double,
    val harmRisk: Double,
    val zone: String,
    val machineAction: String
)

fun clamp01(value: Double): Double = value.coerceIn(0.0, 1.0)

fun assess(
    breachFlowLps: Double,
    surchargeDurationS: Double,
    bankSensitivity: Double,
    distanceM: Double,
    energyreqJ: Double,
    deltaVt: Double
): Assessment {
    require(breachFlowLps > 0.0 && surchargeDurationS > 0.0) {
        "flow and duration must be positive"
    }
    require(bankSensitivity in 0.0..1.0 && distanceM >= 0.0 && energyreqJ >= 0.0 && deltaVt >= 0.0) {
        "sensitivity must be 0..1 and remaining inputs must be non-negative"
    }

    val baseRadiusM = sqrt(breachFlowLps * surchargeDurationS) / 10.0
    val conservativeRadiusM = baseRadiusM * (1.0 + bankSensitivity * 1.5)
    val exposure = if (conservativeRadiusM <= 0.0) 0.0
        else clamp01(1.0 - distanceM / conservativeRadiusM)
    val energyLoad = clamp01(energyreqJ / 1_000_000.0)
    val velocityLoad = clamp01(deltaVt / 10.0)
    val harmRisk = clamp01(
        0.60 * exposure + 0.20 * bankSensitivity + 0.10 * energyLoad + 0.10 * velocityLoad
    )
    val knowledgeFactor = clamp01(1.0 - 0.35 * bankSensitivity - 0.25 * energyLoad)
    val ecoImpactValue = clamp01((1.0 - harmRisk) * (0.40 + 0.60 * knowledgeFactor))

    return when {
        harmRisk >= 0.60 -> Assessment(
            baseRadiusM, conservativeRadiusM, knowledgeFactor, ecoImpactValue,
            harmRisk, "EXCLUDE", "NO_ENTRY"
        )
        harmRisk > 0.25 -> Assessment(
            baseRadiusM, conservativeRadiusM, knowledgeFactor, ecoImpactValue,
            harmRisk, "CAUTION", "HOLD_FOR_INSPECTION"
        )
        else -> Assessment(
            baseRadiusM, conservativeRadiusM, knowledgeFactor, ecoImpactValue,
            harmRisk, "SAFE", "OPERATE_LOW_IMPACT"
        )
    }
}

fun main(args: Array<String>) {
    require(args.size == 6) {
        "usage: CanalBlastRadiusKt <breach_flow_lps> <surcharge_duration_s> " +
            "<bank_sensitivity_0_to_1> <distance_m> <energyreqJ> <delta_vt>"
    }

    val result = assess(
        args[0].toDouble(),
        args[1].toDouble(),
        args[2].toDouble(),
        args[3].toDouble(),
        args[4].toDouble(),
        args[5].toDouble()
    )

    println("base_radius_m=%.3f".format(result.baseRadiusM))
    println("conservative_radius_m=%.3f".format(result.conservativeRadiusM))
    println("knowledge_factor=%.3f".format(result.knowledgeFactor))
    println("eco_impact_value=%.3f".format(result.ecoImpactValue))
    println("harm_risk=%.3f".format(result.harmRisk))
    println("zone=${result.zone}")
    println("machine_action=${result.machineAction}")
}
