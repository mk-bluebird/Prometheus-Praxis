// File: java/cyboquatic/src/main/java/org/prometheuspraxis/cyboquatic/WorkloadTelemetry.java
package org.prometheuspraxis.cyboquatic;

public final class WorkloadTelemetry {
    public record Sample(
            String nodeId, double flowM3s, double liftM, double efficiency,
            double runtimeS, double voltageDropV, double renewableFraction,
            double embodiedCarbonGPerJ, double biodiversityRisk) {}

    public record Assessment(
            double energyReqJ, double deltaVt, double knowledgeFactor,
            double ecoImpactValue, boolean accepted) {}

    public static Assessment assess(Sample t) {
        if (t.nodeId() == null || t.nodeId().isBlank() || t.flowM3s() < 0.0 ||
            t.liftM() < 0.0 || t.efficiency() <= 0.0 || t.efficiency() > 1.0 ||
            t.runtimeS() < 0.0 || t.voltageDropV() < 0.0 ||
            t.renewableFraction() < 0.0 || t.renewableFraction() > 1.0 ||
            t.embodiedCarbonGPerJ() < 0.0 || t.biodiversityRisk() < 0.0 ||
            t.biodiversityRisk() > 1.0) {
            throw new IllegalArgumentException("Invalid cyboquatic telemetry");
        }
        double energy = 997.0 * 9.80665 * t.flowM3s() * t.liftM() * t.runtimeS() / t.efficiency();
        double carbon = energy * (1.0 - t.renewableFraction()) * t.embodiedCarbonGPerJ();
        double delta = 0.55 * Math.min(1.0, carbon / 1000.0)
                + 0.30 * Math.min(1.0, t.voltageDropV() / 24.0)
                + 0.15 * t.biodiversityRisk();
        double knowledge = Math.max(0.0, Math.min(1.0, 1.0 - 0.5 * t.biodiversityRisk()));
        double eco = Math.max(0.0, Math.min(1.0,
                (0.55 * t.renewableFraction() + 0.45 * (1.0 - delta)) *
                (1.0 - t.biodiversityRisk())));
        return new Assessment(energy, delta, knowledge, eco,
                delta <= 0.35 && knowledge >= 0.75 && eco >= 0.60);
    }

    public static void main(String[] args) {
        Assessment a = assess(new Sample("phoenix-canal-pump-01", 0.035, 4.2,
                0.78, 900.0, 2.1, 0.82, 0.000035, 0.08));
        System.out.printf("energyreqJ=%.6f%ndeltaVt=%.6f%necoImpact=%.6f%naccepted=%b%n",
                a.energyReqJ(), a.deltaVt(), a.ecoImpactValue(), a.accepted());
    }
}
