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
        double measurementCompleteness = 
                (t.flowM3s() > 0.0 ? 0.25 : 0.0) +
                (t.liftM() >= 0.0 ? 0.25 : 0.0) +
                (t.runtimeS() > 0.0 ? 0.25 : 0.0) +
                (t.efficiency() > 0.0 ? 0.25 : 0.0);
        double knowledge = Math.max(0.0, Math.min(1.0, measurementCompleteness * (1.0 - 0.5 * t.biodiversityRisk())));
        double eco = Math.max(0.0, Math.min(1.0,
                (0.55 * t.renewableFraction() + 0.45 * (1.0 - delta)) *
                (1.0 - t.biodiversityRisk())));
        return new Assessment(energy, delta, knowledge, eco,
                delta <= 0.35 && knowledge >= 0.75 && eco >= 0.60);
    }

    private static void printUsage() {
        System.err.println("Usage: java org.prometheuspraxis.cyboquatic.WorkloadTelemetry node_id flow_m3_s lift_m efficiency runtime_s voltage_drop_v renewable_fraction embodied_carbon_g_per_j biodiversity_risk");
        System.err.println("  node_id               : non-empty string");
        System.err.println("  flow_m3_s             : >= 0.0");
        System.err.println("  lift_m                : >= 0.0");
        System.err.println("  efficiency            : (0.0, 1.0]");
        System.err.println("  runtime_s             : >= 0.0");
        System.err.println("  voltage_drop_v        : >= 0.0");
        System.err.println("  renewable_fraction    : [0.0, 1.0]");
        System.err.println("  embodied_carbon_g_per_j : >= 0.0");
        System.err.println("  biodiversity_risk     : [0.0, 1.0]");
    }

    public static void main(String[] args) {
        Sample sample;
        if (args.length == 0) {
            sample = new Sample("phoenix-canal-pump-01", 0.035, 4.2,
                    0.78, 900.0, 2.1, 0.82, 0.000035, 0.08);
        } else if (args.length == 9) {
            try {
                sample = new Sample(
                        args[0],
                        Double.parseDouble(args[1]),
                        Double.parseDouble(args[2]),
                        Double.parseDouble(args[3]),
                        Double.parseDouble(args[4]),
                        Double.parseDouble(args[5]),
                        Double.parseDouble(args[6]),
                        Double.parseDouble(args[7]),
                        Double.parseDouble(args[8])
                );
            } catch (NumberFormatException e) {
                printUsage();
                System.exit(1);
                return;
            }
        } else {
            printUsage();
            System.exit(1);
            return;
        }

        Assessment a = assess(sample);
        System.out.printf("node_id=%s%nenergyreqJ=%.6f%ndeltaVt=%.6f%nknowledge_factor=%.6f%neco_impact_value=%.6f%naccepted=%d%n",
                a.accepted() ? sample.nodeId() : sample.nodeId(),
                a.energyReqJ(), a.deltaVt(), a.knowledgeFactor(), a.ecoImpactValue(),
                a.accepted() ? 1 : 0);
        System.exit(a.accepted() ? 0 : 2);
    }
}
