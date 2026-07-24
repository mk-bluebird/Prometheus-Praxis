// ecorestorationshard/cyboquaticprogress/20260723-d-cyboquaticworkload/kotlin/CyboWorkloadWindow.kt
// Kotlin model for cyboquatic workload windows and KER normalization.

package cyboquatics.workload

import java.time.Instant
import kotlin.math.max
import kotlin.math.min
import kotlin.math.abs

data class CyboWorkloadWindow(
    val frameId: String,
    val nodeId: String,
    val timestampUtc: Instant,
    val energyReqJ: Double,
    val energyCorridorMaxJ: Double,
    val hydraulicLoad: Double,
    val hydraulicCorridorMax: Double,
    val carbonIntensity: Double,
    val carbonCorridorMax: Double,
    val uncertaintyRaw: Double,
    val vtBefore: Double,
    val vtAfter: Double,
    val k: Double,
    val e: Double,
    val r: Double,
    val kerScore: Double,
    val fogRegionId: String,
    val fogChannelId: String,
    val governanceParticleHex: String
) {
    init {
        require(frameId.isNotBlank()) { "frameId must not be blank" }
        require(nodeId.isNotBlank()) { "nodeId must not be blank" }
        require(fogRegionId.isNotBlank()) { "fogRegionId must not be blank" }
        require(fogChannelId.isNotBlank()) { "fogChannelId must not be blank" }
        require(governanceParticleHex.isNotBlank()) { "governanceParticleHex must not be blank" }

        require(energyReqJ >= 0.0) { "energyReqJ must be non-negative" }
        require(energyCorridorMaxJ > 0.0) { "energyCorridorMaxJ must be positive" }
        require(hydraulicCorridorMax > 0.0) { "hydraulicCorridorMax must be positive" }
        require(carbonCorridorMax > 0.0) { "carbonCorridorMax must be positive" }

        enforceUnitInterval("k", k)
        enforceUnitInterval("e", e)
        enforceUnitInterval("r", r)

        val expectedKer = computeKerScore(k, e, r)
        require(closeTo(expectedKer, kerScore)) {
            "kerScore=$kerScore inconsistent with expected=$expectedKer"
        }

        require(kerScore > 0.0) { "kerScore must be positive" }
    }

    companion object {
        fun computeKerScore(k: Double, e: Double, r: Double): Double {
            val ck = constrainUnit(k)
            val ce = constrainUnit(e)
            val cr = constrainUnit(r)
            return ck * ce - cr
        }

        private fun enforceUnitInterval(name: String, value: Double) {
            require(value in 0.0..1.0) { "$name must be in [0,1], got $value" }
        }

        private fun constrainUnit(value: Double): Double {
            return max(0.0, min(1.0, value))
        }

        private fun closeTo(a: Double, b: Double, tolerance: Double = 1e-6): Boolean {
            return abs(a - b) <= tolerance
        }
    }
}
