// File: kotlin/cyboquatic_workload_20260807/CyboquaticGate.kt
package cyboquatic.workload

data class WorkloadInput(
    val energyReqJ: Double,
    val deltaVt: Double,
    val renewableFraction: Double,
    val recoveredEnergyJ: Double,
    val waterQualityGain: Double,
    val fogState: String
)

object CyboquaticGate {
    fun accepts(input: WorkloadInput): Boolean {
        val netEnergy = (input.energyReqJ - input.recoveredEnergyJ).coerceAtLeast(0.0)
        return input.fogState == "CLEAR" &&
            input.deltaVt <= 0.0 &&
            input.renewableFraction in 0.70..1.0 &&
            input.waterQualityGain in 0.20..1.0 &&
            input.recoveredEnergyJ in 0.0..input.energyReqJ &&
            netEnergy <= 50000.0
    }
}
