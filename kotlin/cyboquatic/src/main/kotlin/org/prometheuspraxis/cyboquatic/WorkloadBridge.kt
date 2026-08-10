// File: kotlin/cyboquatic/src/main/kotlin/org/prometheuspraxis/cyboquatic/WorkloadBridge.kt
package org.prometheuspraxis.cyboquatic

data class WorkloadFrame(
    val nodeId: String, val energyReqJ: Double, val deltaVt: Double,
    val knowledgeFactor: Double, val ecoImpactValue: Double
) {
    init {
        require(nodeId.isNotBlank())
        require(energyReqJ >= 0.0 && deltaVt in 0.0..1.0)
        require(knowledgeFactor in 0.0..1.0 && ecoImpactValue in 0.0..1.0)
    }

    fun sqlValues(): String =
        "('${nodeId.replace("'", "''")}', $energyReqJ, $deltaVt, $knowledgeFactor, $ecoImpactValue)"
}
