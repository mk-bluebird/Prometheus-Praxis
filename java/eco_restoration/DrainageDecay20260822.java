// Repository: mk-bluebird/Prometheus-Praxis
// Filename: java/eco_restoration/DrainageDecay20260822.java
// Destination: java/eco_restoration/

import java.util.Locale;

public final class DrainageDecay20260822 {
    private static final double CEC_CAPACITY_CMOL_KG = 60.0;

    private record DrainageFrame(
        double hours,
        double bodMgL,
        double tssMgL,
        double cecCmolKg,
        double energyreqJ,
        double deltaVt
    ) {}

    private record KerScore(
        double knowledgeFactor,
        double ecoImpactValue,
        double harmRisk
    ) {}

    private static void requireRange(String name, double value, double minimum, double maximum) {
        if (!Double.isFinite(value) || value < minimum || value > maximum) {
            throw new IllegalArgumentException(name + " is outside its permitted range");
        }
    }

    private static double clamp(double value, double minimum, double maximum) {
        return Math.max(minimum, Math.min(maximum, value));
    }

    private static DrainageFrame projectFrame(
        double hours,
        double initialBodMgL,
        double initialTssMgL,
        double initialCecCmolKg,
        double bodDecayPerHour,
        double tssDecayPerHour,
        double cecRecoveryPerHour,
        double energyreqJ,
        double deltaVt
    ) {
        requireRange("hours", hours, 0.0, 24.0 * 365.0);
        requireRange("initialBodMgL", initialBodMgL, 0.0, 100000.0);
        requireRange("initialTssMgL", initialTssMgL, 0.0, 100000.0);
        requireRange("initialCecCmolKg", initialCecCmolKg, 0.0, 200.0);
        requireRange("bodDecayPerHour", bodDecayPerHour, 0.0, 1.0);
        requireRange("tssDecayPerHour", tssDecayPerHour, 0.0, 1.0);
        requireRange("cecRecoveryPerHour", cecRecoveryPerHour, 0.0, 1.0);
        requireRange("energyreqJ", energyreqJ, 0.0, 1.0e12);
        requireRange("deltaVt", deltaVt, -1000.0, 1000.0);

        double bodMgL = initialBodMgL * Math.exp(-bodDecayPerHour * hours);
        double tssMgL = initialTssMgL * Math.exp(-tssDecayPerHour * hours);
        double cecCmolKg = CEC_CAPACITY_CMOL_KG
            - (CEC_CAPACITY_CMOL_KG - initialCecCmolKg)
            * Math.exp(-cecRecoveryPerHour * hours);

        return new DrainageFrame(hours, bodMgL, tssMgL, cecCmolKg, energyreqJ, deltaVt);
    }

    private static KerScore scoreFrame(DrainageFrame frame, double sampleCompleteness) {
        requireRange("sampleCompleteness", sampleCompleteness, 0.0, 1.0);

        double bodQuality = clamp(1.0 - frame.bodMgL() / 30.0, 0.0, 1.0);
        double tssQuality = clamp(1.0 - frame.tssMgL() / 30.0, 0.0, 1.0);
        double cecQuality = clamp(frame.cecCmolKg() / 30.0, 0.0, 1.0);
        double energyQuality = clamp(1.0 - frame.energyreqJ() / 5.0e6, 0.0, 1.0);
        double voltageStability = clamp(1.0 - Math.abs(frame.deltaVt()) / 24.0, 0.0, 1.0);

        double knowledgeFactor = clamp(
            0.65 * sampleCompleteness + 0.35 * voltageStability, 0.0, 1.0
        );
        double ecoImpactValue = clamp(
            0.35 * bodQuality + 0.30 * tssQuality + 0.20 * cecQuality + 0.15 * energyQuality,
            0.0, 1.0
        );
        double harmRisk = clamp(
            1.0 - (0.40 * bodQuality + 0.35 * tssQuality + 0.15 * voltageStability
                + 0.10 * energyQuality),
            0.0, 1.0
        );

        return new KerScore(knowledgeFactor, ecoImpactValue, harmRisk);
    }

    public static void main(String[] args) {
        if (args.length != 9) {
            System.err.println(
                "Usage: java DrainageDecay20260822"
                    + " hours initial_bod_mg_l initial_tss_mg_l initial_cec_cmol_kg"
                    + " bod_decay_per_hour tss_decay_per_hour cec_recovery_per_hour"
                    + " energyreq_j delta_vt"
            );
            System.exit(64);
        }

        try {
            DrainageFrame frame = projectFrame(
                Double.parseDouble(args[0]), Double.parseDouble(args[1]),
                Double.parseDouble(args[2]), Double.parseDouble(args[3]),
                Double.parseDouble(args[4]), Double.parseDouble(args[5]),
                Double.parseDouble(args[6]), Double.parseDouble(args[7]),
                Double.parseDouble(args[8])
            );
            KerScore ker = scoreFrame(frame, 1.0);

            System.out.printf(Locale.ROOT, "hours=%.6f%n", frame.hours());
            System.out.printf(Locale.ROOT, "bod_mg_l=%.6f%n", frame.bodMgL());
            System.out.printf(Locale.ROOT, "tss_mg_l=%.6f%n", frame.tssMgL());
            System.out.printf(Locale.ROOT, "cec_cmol_kg=%.6f%n", frame.cecCmolKg());
            System.out.printf(Locale.ROOT, "energyreq_j=%.6f%n", frame.energyreqJ());
            System.out.printf(Locale.ROOT, "delta_vt=%.6f%n", frame.deltaVt());
            System.out.printf(Locale.ROOT, "knowledge_factor=%.6f%n", ker.knowledgeFactor());
            System.out.printf(Locale.ROOT, "eco_impact_value=%.6f%n", ker.ecoImpactValue());
            System.out.printf(Locale.ROOT, "harm_risk=%.6f%n", ker.harmRisk());
        } catch (RuntimeException exception) {
            System.err.println("Input error: " + exception.getMessage());
            System.exit(65);
        }
    }
}
