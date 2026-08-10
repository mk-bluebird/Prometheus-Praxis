// File: kotlin/cyboquatic/src/main/kotlin/org/prometheuspraxis/cyboquatic/WorkloadBridge.kt
package org.prometheuspraxis.cyboquatic

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.memScoped
import org.prometheuspraxis.cyboquatic.nativecore.WorkloadAssessment
import org.prometheuspraxis.cyboquatic.nativecore.WorkloadInput
import org.prometheuspraxis.cyboquatic.nativecore.assess_workload

data class WorkloadSample(
    val nodeId: String,
    val flowM3s: Double,
    val liftM: Double,
    val efficiency: Double,
    val runtimeS: Double,
    val voltageDropV: Double,
    val renewableFraction: Double,
    val embodiedCarbonGPerJ: Double,
    val biodiversityRisk: Double
) {
    init {
        require(nodeId.isNotBlank())
        require(flowM3s.isFinite() && flowM3s >= 0.0)
        require(liftM.isFinite() && liftM >= 0.0)
        require(efficiency.isFinite() && efficiency > 0.0 && efficiency <= 1.0)
        require(runtimeS.isFinite() && runtimeS >= 0.0)
        require(voltageDropV.isFinite() && voltageDropV >= 0.0)
        require(renewableFraction.isFinite() && renewableFraction in 0.0..1.0)
        require(embodiedCarbonGPerJ.isFinite() && embodiedCarbonGPerJ >= 0.0)
        require(biodiversityRisk.isFinite() && biodiversityRisk in 0.0..1.0)
    }
}

data class WorkloadFrame(
    val nodeId: String,
    val energyReqJ: Double,
    val deltaVt: Double,
    val knowledgeFactor: Double,
    val ecoImpactValue: Double,
    val accepted: Boolean
) {
    init {
        require(nodeId.isNotBlank())
        require(energyReqJ.isFinite() && energyReqJ >= 0.0)
        require(deltaVt.isFinite() && deltaVt in 0.0..1.0)
        require(knowledgeFactor.isFinite() && knowledgeFactor in 0.0..1.0)
        require(ecoImpactValue.isFinite() && ecoImpactValue in 0.0..1.0)
    }

    fun sqlValues(): String {
        val escapedNodeId = nodeId.replace("'", "''")
        return "('$escapedNodeId', $energyReqJ, $deltaVt, $knowledgeFactor, $ecoImpactValue, ${if (accepted) 1 else 0})"
    }
}

@OptIn(ExperimentalForeignApi::class)
object WorkloadBridge {
    fun toWorkloadFrame(sample: WorkloadSample): WorkloadFrame = memScoped {
        val input = alloc<WorkloadInput>()
        input.flow_m3_s = sample.flowM3s
        input.lift_m = sample.liftM
        input.efficiency = sample.efficiency
        input.runtime_s = sample.runtimeS
        input.voltage_drop_v = sample.voltageDropV
        input.renewable_fraction = sample.renewableFraction
        input.embodied_carbon_g_per_j = sample.embodiedCarbonGPerJ
        input.biodiversity_risk = sample.biodiversityRisk

        val output = alloc<WorkloadAssessment>()
        check(assess_workload(input.ptr, output.ptr) == 0) {
            "cyboquatic-core rejected validated workload telemetry"
        }

        WorkloadFrame(
            nodeId = sample.nodeId,
            energyReqJ = output.energyreq_j,
            deltaVt = output.delta_vt,
            knowledgeFactor = output.knowledge_factor,
            ecoImpactValue = output.eco_impact_value,
            accepted = output.accepted.toInt() != 0
        )
    }
}
