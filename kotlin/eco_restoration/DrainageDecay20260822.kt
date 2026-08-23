// Repository: mk-bluebird/Prometheus-Praxis
// Filename: kotlin/eco_restoration/DrainageDecay20260822.kt
// Destination: kotlin/eco_restoration/

import kotlin.math.abs
import kotlin.math.exp

data class DrainageFrame(
    val hours: Double,
    val bodMgL: Double,
    val tssMgL: Double,
    val cecCmolKg: Double,
    val energyreqJ: Double,
    val deltaVt: Double
)

data class KerScore(
    val knowledgeFactor: Double,
    val ecoImpactValue: Double,
    val harmRisk: Double
)

private const val CEC_CAPACITY_CMOL_KG = 60.0

private fun requireRange(name: String, value: Double, minimum: Double, maximum: Double) {
    require(value.isFinite() && value in minimum..maximum) {
        "$name is outside its permitted range"
    }
}

private fun projectFrame(
    hours: Double,
    initialBodMgL: Double,
    initialTssMgL: Double,
    initialCecCmolKg: Double,
    bodDecayPerHour: Double,
    tssDecayPerHour: Double,
    cecRecoveryPerHour: Double,
    energyreqJ: Double,
    deltaVt: Double
): DrainageFrame {
    requireRange("hours", hours, 0.0, 24.0 * 365.0)
    requireRange("initialBodMgL", initialBodMgL, 0.0, 100000.0)
    requireRange("initialTssMgL", initialTssMgL, 0.0, 100000.0)
    requireRange("initialCecCmolKg", initialCecCmolKg, 0.0, 200.0)
    requireRange("bodDecayPerHour", bodDecayPerHour, 0.0, 1.0)
    requireRange("tssDecayPerHour", tssDecayPerHour, 0.0, 1.0)
    requireRange("cecRecoveryPerHour", cecRecoveryPerHour, 0.0, 1.0)
    requireRange("energyreqJ", energyreqJ, 0.0, 1.0e12)
    requireRange("deltaVt", deltaVt, -1000.0, 1000.0)

    return DrainageFrame(
        hours = hours,
        bodMgL = initialBodMgL * exp(-bodDecayPerHour * hours),
        tssMgL = initialTssMgL * exp(-tssDecayPerHour * hours),
        cecCmolKg = CEC_CAPACITY_CMOL_KG -
            (CEC_CAPACITY_CMOL_KG - initialCecCmolKg) * exp(-cecRecoveryPerHour * hours),
        energyreqJ = energyreqJ,
        deltaVt = deltaVt
    )
}

private fun scoreFrame(frame: DrainageFrame, sampleCompleteness: Double): KerScore {
    requireRange("sampleCompleteness", sampleCompleteness, 0.0, 1.0)

    val bodQuality = (1.0 - frame.bodMgL / 30.0).coerceIn(0.0, 1.0)
    val tssQuality = (1.0 - frame.tssMgL / 30.0).coerceIn(0.0, 1.0)
    val cecQuality = (frame.cecCmolKg / 30.0).coerceIn(0.0, 1.0)
    val energyQuality = (1.0 - frame.energyreqJ / 5.0e6).coerceIn(0.0, 1.0)
    val voltageStability = (1.0 - abs(frame.deltaVt) / 24.0).coerceIn(0.0, 1.0)

    return KerScore(
        knowledgeFactor = (0.65 * sampleCompleteness + 0.35 * voltageStability).coerceIn(0.0, 1.0),
        ecoImpactValue = (
            0.35 * bodQuality + 0.30 * tssQuality + 0.20 * cecQuality + 0.15 * energyQuality
        ).coerceIn(0.0, 1.0),
        harmRisk = (
            1.0 - (0.40 * bodQuality + 0.35 * tssQuality + 0.15 * voltageStability
                + 0.10 * energyQuality)
        ).coerceIn(0.0, 1.0)
    )
}

fun main(args: Array<String>) {
    require(args.size == 9) {
        "Usage: DrainageDecay20260822 hours initial_bod_mg_l initial_tss_mg_l initial_cec_cmol_kg " +
            "bod_decay_per_hour tss_decay_per_hour cec_recovery_per_hour energyreq_j delta_vt"
    }

    val frame = projectFrame(
        args[0].toDouble(), args[1].toDouble(), args[2].toDouble(), args[3].toDouble(),
        args[4].toDouble(), args[5].toDouble(), args[6].toDouble(), args[7].toDouble(),
        args[8].toDouble()
    )
    val ker = scoreFrame(frame, 1.0)

    println("hours=%.6f".format(frame.hours))
    println("bod_mg_l=%.6f".format(frame.bodMgL))
    println("tss_mg_l=%.6f".format(frame.tssMgL))
    println("cec_cmol_kg=%.6f".format(frame.cecCmolKg))
    println("energyreq_j=%.6f".format(frame.energyreqJ))
    println("delta_vt=%.6f".format(frame.deltaVt))
    println("knowledge_factor=%.6f".format(ker.knowledgeFactor))
    println("eco_impact_value=%.6f".format(ker.ecoImpactValue))
    println("harm_risk=%.6f".format(ker.harmRisk))
}
