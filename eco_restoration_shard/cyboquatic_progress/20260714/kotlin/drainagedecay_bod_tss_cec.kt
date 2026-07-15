// File: eco_restoration_shard/cyboquatic_progress/20260714/kotlin/drainagedecay_bod_tss_cec.kt
// Domain (e): drainagedecay frames (BOD, TSS, CEC) for cyboquatic machinery.
// This Kotlin code is JVM-ready and can be compiled with a standard Kotlin toolchain (no external dependencies).

package cyboquatic.drainagedecay

import kotlin.math.exp

/**
 * DrainageState captures instantaneous water quality state in a cyboquatic channel.
 * Units:
 *  - bodMgL: Biochemical Oxygen Demand, mg O2/L
 *  - tssMgL: Total Suspended Solids, mg/L
 *  - cecCmolKg: Cation Exchange Capacity, cmol(+)/kg of substrate
 *  - temperatureC: Water temperature, degrees Celsius
 *  - flowLps: Flow rate, liters per second
 */
data class DrainageState(
    val bodMgL: Double,
    val tssMgL: Double,
    val cecCmolKg: Double,
    val temperatureC: Double,
    val flowLps: Double
)

/**
 * DecayParameters holds first-order decay coefficients for BOD and TSS.
 * These can be field-calibrated; defaults here are conservative and low-risk.
 * Units:
 *  - kBodPerDay: day^-1
 *  - kTssPerDay: day^-1
 *  - theta: dimensionless temperature correction factor (Arrhenius-type)
 *  - refTempC: reference temperature, degrees Celsius
 */
data class DecayParameters(
    val kBodPerDay: Double = 0.15,
    val kTssPerDay: Double = 0.05,
    val theta: Double = 1.047,
    val refTempC: Double = 20.0
)

/**
 * DrainageDecayFrame computes next-step BOD/TSS/CEC given current state and a time step.
 * Model:
 *  - First-order decay for BOD and TSS with simple temperature correction
 *  - CEC is assumed constant over short time steps but exposed for future sorption models
 * The design focuses on:
 *  - Energy-efficiency: no heap churn, simple arithmetic, safe for low-power controllers
 *  - Carbon-negative routing: encourages designs that minimize aeration demand by tracking BOD
 */
object DrainageDecayFrame {

    /**
     * Compute the next drainage state after dtHours.
     * dtHours must be non-negative; negative steps are rejected to avoid unstable time-reversal.
     */
    fun step(
        state: DrainageState,
        params: DecayParameters,
        dtHours: Double
    ): DrainageState {
        require(dtHours >= 0.0) { "dtHours must be non-negative" }

        val tempFactor = temperatureFactor(
            theta = params.theta,
            refTempC = params.refTempC,
            currentTempC = state.temperatureC
        )

        // Convert per-day rates to per-hour.
        val kBodPerHour = params.kBodPerDay / 24.0 * tempFactor
        val kTssPerHour = params.kTssPerDay / 24.0 * tempFactor

        val bodNext = firstOrderDecay(state.bodMgL, kBodPerHour, dtHours)
        val tssNext = firstOrderDecay(state.tssMgL, kTssPerHour, dtHours)

        val bodClamped = bodNext.coerceAtLeast(0.0)
        val tssClamped = tssNext.coerceAtLeast(0.0)

        return state.copy(
            bodMgL = bodClamped,
            tssMgL = tssClamped
        )
    }

    /**
     * Estimate instantaneous oxygen demand rate (mg O2/s) from BOD and flow.
     * Assumes BOD concentration is directly proportional to oxygen requirement.
     */
    fun oxygenDemandMgPerSec(state: DrainageState): Double {
        val bodNonNegative = state.bodMgL.coerceAtLeast(0.0)
        val flowNonNegative = state.flowLps.coerceAtLeast(0.0)
        return bodNonNegative * flowNonNegative / 1000.0
    }

    private fun firstOrderDecay(
        initial: Double,
        kPerHour: Double,
        dtHours: Double
    ): Double {
        if (initial <= 0.0) return 0.0
        if (kPerHour <= 0.0 || dtHours == 0.0) return initial
        val exponent = -kPerHour * dtHours
        return initial * exp(exponent)
    }

    private fun temperatureFactor(
        theta: Double,
        refTempC: Double,
        currentTempC: Double
    ): Double {
        val delta = currentTempC - refTempC
        return theta.pow(delta / 10.0)
    }

    private fun Double.pow(exponent: Double): Double {
        return exp(exponent * kotlin.math.ln(this))
    }
}

/**
 * Simple CLI-style demonstration.
 * This can be invoked with: kotlin -classpath . cyboquatic.drainagedecay.Drainagedecay_bod_tss_cecKt
 */
fun main() {
    val initialState = DrainageState(
        bodMgL = 40.0,
        tssMgL = 80.0,
        cecCmolKg = 25.0,
        temperatureC = 22.0,
        flowLps = 5.0
    )

    val params = DecayParameters(
        kBodPerDay = 0.2,
        kTssPerDay = 0.06,
        theta = 1.05,
        refTempC = 20.0
    )

    val dtHours = 6.0

    val nextState = DrainageDecayFrame.step(initialState, params, dtHours)
    val oxygenDemand = DrainageDecayFrame.oxygenDemandMgPerSec(nextState)

    println("Initial state: $initialState")
    println("Next state after $dtHours h: $nextState")
    println("Oxygen demand (mg O2/s): $oxygenDemand")
}
