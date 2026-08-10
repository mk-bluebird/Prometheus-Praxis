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

/**
 * Converts a [WorkloadTelemetry.Assessment] into a validated [WorkloadFrame].
 * The produced frame contains the shared nodeId, calculated energy, deltaVt,
 * knowledge factor, and eco-impact value. The workload formula is not duplicated
 * here; it relies on the Java assessment logic.
 */
fun WorkloadTelemetry.Assessment.toWorkloadFrame(nodeId: String): WorkloadFrame {
    return WorkloadFrame(
        nodeId = nodeId,
        energyReqJ = this.energyReqJ,
        deltaVt = this.deltaVt,
        knowledgeFactor = this.knowledgeFactor,
        ecoImpactValue = this.ecoImpactValue
    )
}
