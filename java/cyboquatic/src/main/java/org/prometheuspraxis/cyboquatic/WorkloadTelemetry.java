// File: java/cyboquatic/src/main/java/org/prometheuspraxis/cyboquatic/WorkloadTelemetry.java
package org.prometheuspraxis.cyboquatic;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Locale;
import java.util.Objects;

public final class WorkloadTelemetry {
    static {
        loadNativeLibrary();
    }

    private WorkloadTelemetry() {
    }

    public record Sample(
            String nodeId,
            double flowM3s,
            double liftM,
            double efficiency,
            double runtimeS,
            double voltageDropV,
            double renewableFraction,
            double embodiedCarbonGPerJ,
            double biodiversityRisk) {
        public Sample {
            nodeId = Objects.requireNonNull(nodeId, "nodeId").trim();
            if (nodeId.isEmpty()
                    || !nonNegative(flowM3s)
                    || !nonNegative(liftM)
                    || !finite(efficiency) || efficiency <= 0.0 || efficiency > 1.0
                    || !nonNegative(runtimeS)
                    || !nonNegative(voltageDropV)
                    || !unitInterval(renewableFraction)
                    || !nonNegative(embodiedCarbonGPerJ)
                    || !unitInterval(biodiversityRisk)) {
                throw new IllegalArgumentException("Invalid cyboquatic telemetry");
            }
        }
    }

    public record Assessment(
            double energyReqJ,
            double deltaVt,
            double knowledgeFactor,
            double ecoImpactValue,
            boolean accepted) {
        public Assessment {
            if (!nonNegative(energyReqJ)
                    || !unitInterval(deltaVt)
                    || !unitInterval(knowledgeFactor)
                    || !unitInterval(ecoImpactValue)) {
                throw new IllegalStateException("Native workload assessment violated output bounds");
            }
        }
    }

    private static native Assessment nativeAssess(
            double flowM3s,
            double liftM,
            double efficiency,
            double runtimeS,
            double voltageDropV,
            double renewableFraction,
            double embodiedCarbonGPerJ,
            double biodiversityRisk);

    public static Assessment assess(Sample sample) {
        Objects.requireNonNull(sample, "sample");
        return nativeAssess(
                sample.flowM3s(),
                sample.liftM(),
                sample.efficiency(),
                sample.runtimeS(),
                sample.voltageDropV(),
                sample.renewableFraction(),
                sample.embodiedCarbonGPerJ(),
                sample.biodiversityRisk());
    }

    private static void loadNativeLibrary() {
        String configuredPath = System.getProperty("cyboquatic.core.library", "").trim();
        if (!configuredPath.isEmpty()) {
            Path library = Path.of(configuredPath).toAbsolutePath().normalize();
            if (!Files.isRegularFile(library)) {
                throw new UnsatisfiedLinkError(
                        "cyboquatic.core.library is not a readable file: " + library);
            }
            System.load(library.toString());
            return;
        }
        System.loadLibrary("cyboquatic_core");
    }

    private static boolean finite(double value) {
        return Double.isFinite(value);
    }

    private static boolean nonNegative(double value) {
        return finite(value) && value >= 0.0;
    }

    private static boolean unitInterval(double value) {
        return finite(value) && value >= 0.0 && value <= 1.0;
    }

    private static void printUsage() {
        System.err.println(
                "Usage: WorkloadTelemetry node_id flow_m3_s lift_m efficiency runtime_s "
                        + "voltage_drop_v renewable_fraction embodied_carbon_g_per_j "
                        + "biodiversity_risk");
        System.err.println(
                "Optional native library path: "
                        + "-Dcyboquatic.core.library=/absolute/path/to/libcyboquatic_core");
    }

    private static Sample parseSample(String[] args) {
        if (args.length == 0) {
            return new Sample(
                    "phoenix-canal-pump-01", 0.035, 4.2, 0.78,
                    900.0, 2.1, 0.82, 0.000035, 0.08);
        }
        if (args.length != 9) {
            throw new IllegalArgumentException("Expected nine telemetry arguments");
        }
        return new Sample(
                args[0],
                Double.parseDouble(args[1]),
                Double.parseDouble(args[2]),
                Double.parseDouble(args[3]),
                Double.parseDouble(args[4]),
                Double.parseDouble(args[5]),
                Double.parseDouble(args[6]),
                Double.parseDouble(args[7]),
                Double.parseDouble(args[8]));
    }

    public static void main(String[] args) {
        try {
            Sample sample = parseSample(args);
            Assessment assessment = assess(sample);
            System.out.printf(
                    Locale.ROOT,
                    "node_id=%s%nenergyreqJ=%.6f%ndeltaVt=%.6f%n"
                            + "knowledge_factor=%.6f%neco_impact_value=%.6f%naccepted=%d%n",
                    sample.nodeId(),
                    assessment.energyReqJ(),
                    assessment.deltaVt(),
                    assessment.knowledgeFactor(),
                    assessment.ecoImpactValue(),
                    assessment.accepted() ? 1 : 0);
            System.exit(assessment.accepted() ? 0 : 2);
        } catch (IllegalArgumentException | UnsatisfiedLinkError error) {
            System.err.println(error.getMessage());
            printUsage();
            System.exit(1);
        }
    }
}
