import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

data class FogState(
    val pFog: Int,
    val score: Double,
    val consecutiveSafetyViolations: Int
)

data class HysteresisDecision(
    val nextState: FogState,
    val switched: Boolean,
    val effectiveDelta: Double,
    val routingAction: String,
    val knowledgeFactor: Double,
    val ecoImpactValue: Double,
    val harmRisk: Double
)

fun clamp01(value: Double): Double = max(0.0, min(1.0, value))

fun updateFogState(
    prior: FogState,
    currentScore: Double,
    configuredDelta: Double,
    uncertaintySigma: Double,
    safetyMarginMultiplier: Double,
    safetyViolation: Boolean,
    minimumViolationsForForcedHold: Int
): HysteresisDecision {
    require(prior.pFog == 0 || prior.pFog == 1) { "prior pFog must be 0 or 1" }
    require(currentScore.isFinite()) { "currentScore must be finite" }
    require(configuredDelta >= 0.0) { "configuredDelta must be non-negative" }
    require(uncertaintySigma >= 0.0) { "uncertaintySigma must be non-negative" }
    require(safetyMarginMultiplier >= 0.0) { "safetyMarginMultiplier must be non-negative" }
    require(minimumViolationsForForcedHold >= 1) { "minimumViolationsForForcedHold must be at least one" }

    val effectiveDelta = configuredDelta + safetyMarginMultiplier * uncertaintySigma
    val scoreChange = currentScore - prior.score
    val nextViolations = if (safetyViolation) prior.consecutiveSafetyViolations + 1 else 0
    val forceFog = nextViolations >= minimumViolationsForForcedHold

    val candidateState = if (currentScore > 0.0) 1 else 0
    val switchAllowed = abs(scoreChange) > effectiveDelta
    val nextPFog = when {
        forceFog -> 1
        switchAllowed -> candidateState
        else -> prior.pFog
    }

    val switched = nextPFog != prior.pFog
    val action = when {
        nextPFog == 1 -> "MANUAL_FIELD_REVIEW"
        switchAllowed -> "RETAIN_FOR_SAMPLING_AND_CONTEXT_REVIEW"
        else -> "HOLD_PREVIOUS_ROUTE"
    }

    val risk = when {
        forceFog -> 0.80
        nextPFog == 1 -> 0.70
        abs(scoreChange) <= effectiveDelta -> 0.35
        else -> 0.45
    }

    return HysteresisDecision(
        nextState = FogState(nextPFog, currentScore, nextViolations),
        switched = switched,
        effectiveDelta = effectiveDelta,
        routingAction = action,
        knowledgeFactor = clamp01(0.75 - 0.30 * uncertaintySigma),
        ecoImpactValue = clamp01(0.70 - 0.25 * risk),
        harmRisk = risk
    )
}

fun main(args: Array<String>) {
    require(args.size == 8) {
        "usage: FogHysteresisRouterKt <prior_p_fog> <prior_score> <current_score> <delta> " +
            "<uncertainty_sigma> <safety_multiplier> <safety_violation_0_or_1> <forced_hold_count>"
    }

    val result = updateFogState(
        prior = FogState(args[0].toInt(), args[1].toDouble(), 0),
        currentScore = args[2].toDouble(),
        configuredDelta = args[3].toDouble(),
        uncertaintySigma = args[4].toDouble(),
        safetyMarginMultiplier = args[5].toDouble(),
        safetyViolation = args[6].toInt() == 1,
        minimumViolationsForForcedHold = args[7].toInt()
    )

    println("p_fog=${result.nextState.pFog}")
    println("score=${"%.8f".format(result.nextState.score)}")
    println("effective_delta=${"%.8f".format(result.effectiveDelta)}")
    println("switched=${result.switched}")
    println("routing_action=${result.routingAction}")
    println("knowledge_factor=${"%.8f".format(result.knowledgeFactor)}")
    println("eco_impact_value=${"%.8f".format(result.ecoImpactValue)}")
    println("harm_risk=${"%.8f".format(result.harmRisk)}")
}
