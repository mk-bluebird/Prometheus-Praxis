// File: java/cyboquatic/src/main/java/org/prometheuspraxis/cyboquatic/CyboquaticWorkloadMonitor.java
package org.prometheuspraxis.cyboquatic;

public final class CyboquaticWorkloadMonitor {
    public record CanalBlastStatus(
            String canalNode,
            double energyReqJ,
            BlastRadius.Assessment assessment) {}

    public static CanalBlastStatus assessCanalNode(
            String canalNode,
            double energyReqJ,
            double energyCorridorJ,
            double attenuationMInv,
            double baseRadiusM,
            double biodiversityRisk) {
        if (canalNode == null || canalNode.isBlank()) {
            throw new IllegalArgumentException("canalNode must be nonblank");
        }

        return new CanalBlastStatus(
                canalNode,
                energyReqJ,
                BlastRadius.assess(
                        energyReqJ,
                        energyCorridorJ,
                        attenuationMInv,
                        baseRadiusM,
                        biodiversityRisk));
    }
}
