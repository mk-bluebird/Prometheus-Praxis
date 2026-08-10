// File: java/cyboquatic/src/main/java/org/prometheuspraxis/CyboquaticWorkload.java
package org.prometheuspraxis;

public final class CyboquaticWorkload {
    public record Frame(String nodeId, double massKg, double liftM, double flowM3s,
                        double headM, double durationS, double renewableFraction,
                        double embodiedCarbonKgCo2e) {}
    public record Result(double energyreqJ, double deltaVt, double k, double e, double r,
                         double knowledgeFactor, double ecoImpactValue, boolean accepted) {}

    private static double clamp(double value, double low, double high) {
        return Math.max(low, Math.min(high, value));
    }

    public static Result assess(Frame f) {
        double renewable = clamp(f.renewableFraction(), 0.0, 1.0);
        double lift = Math.max(0.0, f.massKg()) * 9.80665 * Math.max(0.0, f.liftM());
        double hydraulic = 1000.0 * 9.80665 * Math.max(0.0, f.flowM3s())
                * Math.max(0.0, f.headM()) * Math.max(0.0, f.durationS());
        double energy = lift + hydraulic;
        double intensity = energy / Math.max(1.0, f.durationS());
        double carbonRisk = clamp((1.0 - renewable) * intensity / 2500.0
                + f.embodiedCarbonKgCo2e() / 100.0, 0.0, 1.0);
        double energyRisk = clamp(intensity / 3000.0, 0.0, 1.0);
        double deltaVt = 0.55 * energyRisk * energyRisk + 0.45 * carbonRisk * carbonRisk;
        double k = 1.0 - deltaVt;
        double e = renewable * (1.0 - carbonRisk);
        double r = Math.max(energyRisk, carbonRisk);
        double knowledge = clamp(0.55 * k + 0.45 * (1.0 - r), 0.0, 1.0);
        double impact = clamp(0.50 * e + 0.30 * knowledge + 0.20 * (1.0 - deltaVt), 0.0, 1.0);
        return new Result(energy, deltaVt, k, e, r, knowledge, impact, k * e > r && renewable >= 0.70);
    }

    public static void main(String[] args) {
        Result r = assess(new Frame("canal-recovery-pump-08", 120, 2.2, 0.018, 1.4, 1800, 0.92, 3.4));
        System.out.printf("energyreqJ=%.6f delta_vt=%.6f eco_impact=%.6f accepted=%b%n",
                r.energyreqJ(), r.deltaVt(), r.ecoImpactValue(), r.accepted());
    }
}
