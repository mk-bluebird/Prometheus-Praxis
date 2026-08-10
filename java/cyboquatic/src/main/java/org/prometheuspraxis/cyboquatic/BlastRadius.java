// File: java/cyboquatic/src/main/java/org/prometheuspraxis/cyboquatic/BlastRadius.java
package org.prometheuspraxis.cyboquatic;

public final class BlastRadius {
    static {
        System.loadLibrary("blast_radius_kernel");
    }

    public record Assessment(
            double radiusM,
            double normalizedEnergyRisk,
            double ecoImpactValue,
            boolean withinCorridor) {}

    private BlastRadius() {}

    private static native Assessment nativeAssess(
            double energyJ,
            double energyCorridorJ,
            double attenuationMInv,
            double baseRadiusM,
            double biodiversityRisk);

    public static Assessment assess(
            double energyJ,
            double energyCorridorJ,
            double attenuationMInv,
            double baseRadiusM,
            double biodiversityRisk) {
        return nativeAssess(
                energyJ,
                energyCorridorJ,
                attenuationMInv,
                baseRadiusM,
                biodiversityRisk);
    }
}
