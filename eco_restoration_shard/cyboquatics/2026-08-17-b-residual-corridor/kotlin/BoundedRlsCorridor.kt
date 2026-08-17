import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

data class RlsState(val theta: Double, val covariance: Double)
data class RlsResult(
    val state: RlsState,
    val residual: Double,
    val gain: Double,
    val gainBound: Double,
    val corridorRadius: Double,
    val accepted: Boolean
)

fun clamp(value: Double, lower: Double, upper: Double): Double = max(lower, min(upper, value))

fun rlsCorridorUpdate(
    state: RlsState,
    phi: Double,
    observation: Double,
    forgettingFactor: Double,
    covarianceMin: Double,
    covarianceMax: Double,
    residualLimit: Double
): RlsResult {
    require(forgettingFactor > 0.0 && forgettingFactor <= 1.0) { "forgettingFactor must be in (0, 1]" }
    require(covarianceMin > 0.0 && covarianceMax >= covarianceMin) { "invalid covariance bounds" }
    require(state.covariance in covarianceMin..covarianceMax) { "state covariance outside configured bounds" }
    require(residualLimit > 0.0) { "residualLimit must be positive" }

    val denominator = forgettingFactor + phi * state.covariance * phi
    require(denominator >= forgettingFactor) { "invalid RLS denominator" }

    val residual = observation - phi * state.theta
    val gain = state.covariance * phi / denominator
    val rawCovariance = (state.covariance - gain * phi * state.covariance) / forgettingFactor
    val nextCovariance = clamp(rawCovariance, covarianceMin, covarianceMax)
    val gainBound = covarianceMax * abs(phi) / forgettingFactor
    val accepted = abs(residual) <= residualLimit
    val nextTheta = if (accepted) state.theta + gain * residual else state.theta

    val modelErrorBound = if (accepted) abs(residual) else residualLimit
    val corridorRadius = abs(phi) * modelErrorBound + gainBound * residualLimit

    return RlsResult(
        RlsState(nextTheta, nextCovariance),
        residual,
        gain,
        gainBound,
        corridorRadius,
        accepted
    )
}

fun main(args: Array<String>) {
    require(args.size >= 8 && (args.size - 5) % 2 == 0) {
        "usage: BoundedRlsCorridorKt <theta0> <P0> <lambda> <Pmin> <Pmax> <residual_limit> " +
            "<phi> <y> [<phi> <y> ...]"
    }

    var state = RlsState(args[0].toDouble(), args[1].toDouble())
    val lambda = args[2].toDouble()
    val pMin = args[3].toDouble()
    val pMax = args[4].toDouble()
    val residualLimit = args[5].toDouble()

    var index = 6
    var step = 0
    while (index < args.size) {
        val result = rlsCorridorUpdate(
            state,
            args[index].toDouble(),
            args[index + 1].toDouble(),
            lambda,
            pMin,
            pMax,
            residualLimit
        )
        state = result.state

        println(
            "step=$step residual=%.8f gain=%.8f gain_bound=%.8f corridor_radius=%.8f accepted=%s theta=%.8f P=%.8f"
                .format(
                    result.residual,
                    result.gain,
                    result.gainBound,
                    result.corridorRadius,
                    result.accepted,
                    state.theta,
                    state.covariance
                )
        )

        index += 2
        step += 1
    }
}
