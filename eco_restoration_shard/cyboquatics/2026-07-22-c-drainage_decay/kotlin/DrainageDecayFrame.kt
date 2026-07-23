// eco_restoration_shard/cyboquatics/2026-07-22-c-drainage_decay/kotlin/DrainageDecayFrame.kt

package cyboquatics.drainagedecay

import java.time.Instant
import kotlin.math.max
import kotlin.math.min

/**
 * Canonical drainage decay telemetry frame for cyboquatic controllers.
 *
 * This model is intentionally dependency-light for deployment on constrained edge hardware.
 */
data class DrainageDecayFrame(
    val frameId: String,
    val canalNodeId: String,
    val timestamp: Instant,
    val bodMgL: Double,
    val tssMgL: Double,
    val cecCmolPerKg: Double,
    val frameEnergyJ: Double,
    val deltaVtMps: Double,
    val kKnowledgeFactor: Double,
    val eEcoImpact: Double,
    val rRiskFactor: Double,
    val kerScore: Double,
    val fogRegionId: String,
    val fogChannelId: String,
    val governanceParticleHex: String
) {
    init {
        require(frameId.isNotBlank()) { "frameId must not be blank" }
        require(canalNodeId.isNotBlank()) { "canalNodeId must not be blank" }
        require(fogRegionId.isNotBlank()) { "fogRegionId must not be blank" }
        require(fogChannelId.isNotBlank()) { "fogChannelId must not be blank" }
        require(governanceParticleHex.isNotBlank()) { "governanceParticleHex must not be blank" }

        // Ecological parameter ranges (can be tuned per deployment)
        require(bodMgL in 0.0..80.0) {
            "BOD must be in [0, 80] mg/L to protect biological communities"
        }
        require(tssMgL in 0.0..500.0) {
            "TSS must be in [0, 500] mg/L to prevent equipment fouling and turbidity overload"
        }
        require(cecCmolPerKg in 0.0..100.0) {
            "CEC must be in [0, 100] cmol(+)/kg to reflect realistic soil and media conditions"
        }

        // Energy discipline: ensure positive and bounded energy values
        require(frameEnergyJ >= 0.0) {
            "frameEnergyJ must be non-negative to maintain energy accounting"
        }
        require(deltaVtMps in -5.0..5.0) {
            "deltaVtMps must be in [-5, 5] m/s to avoid unsafe flow transitions"
        }

        // KER discipline
        enforceUnitInterval("kKnowledgeFactor", kKnowledgeFactor)
        enforceUnitInterval("eEcoImpact", eEcoImpact)
        enforceUnitInterval("rRiskFactor", rRiskFactor)

        val expectedKer = computeKerScore(kKnowledgeFactor, eEcoImpact, rRiskFactor)
        require(closeTo(expectedKer, kerScore)) {
            "kerScore=$kerScore does not match expected=$expectedKer for KER triad"
        }

        // Governance: reject negative or zero kerScore
        require(kerScore > 0.0) {
            "kerScore must be positive; negative or zero scores are not permitted for actuation"
        }
    }

    companion object {

        fun computeKerScore(k: Double, e: Double, r: Double): Double {
            val constrainedK = constrainUnit(k)
            val constrainedE = constrainUnit(e)
            val constrainedR = constrainUnit(r)
            return constrainedK * (constrainedE - constrainedR)
        }

        private fun enforceUnitInterval(name: String, value: Double) {
            require(value in 0.0..1.0) {
                "$name must be in [0, 1]; got $value"
            }
        }

        private fun constrainUnit(value: Double): Double {
            return max(0.0, min(1.0, value))
        }

        private fun closeTo(a: Double, b: Double, tolerance: Double = 1e-6): Boolean {
            return kotlin.math.abs(a - b) <= tolerance
        }
    }
}

/**
 * Simple validator utility for stream-level checks.
 */
object DrainageDecayValidator {

    data class ValidationResult(
        val valid: Boolean,
        val message: String? = null
    )

    /**
     * Validates that a frame respects an energy envelope.
     *
     * @param frame The frame to validate.
     * @param maxFrameEnergyJ Maximum acceptable energy per frame.
     * @param minKerScore Minimum acceptable KER score.
     */
    fun validateEnergyAndKer(
        frame: DrainageDecayFrame,
        maxFrameEnergyJ: Double,
        minKerScore: Double
    ): ValidationResult {
        if (frame.frameEnergyJ > maxFrameEnergyJ) {
            return ValidationResult(
                valid = false,
                message = "Frame exceeds energy envelope: ${frame.frameEnergyJ} J > $maxFrameEnergyJ J"
            )
        }
        if (frame.kerScore < minKerScore) {
            return ValidationResult(
                valid = false,
                message = "Frame KER score too low: ${frame.kerScore} < $minKerScore"
            )
        }
        return ValidationResult(valid = true)
    }
}
