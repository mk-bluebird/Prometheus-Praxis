// File: android/app/src/main/java/org/prometheuspraxis/cyboquatic/WorkloadFrame.kt
package org.prometheuspraxis.cyboquatic

data class WorkloadFrame(
    val canalNode: String,
    val observedUtc: String,
    val energyReqJ: Double,
    val deltaVt: Double,
    val ecoImpactValue: Double,
    val accepted: Boolean
) {
    init {
        require(canalNode.isNotBlank())
        require(energyReqJ >= 0.0)
        require(deltaVt in 0.0..1.0)
        require(ecoImpactValue in 0.0..1.0)
    }
}
