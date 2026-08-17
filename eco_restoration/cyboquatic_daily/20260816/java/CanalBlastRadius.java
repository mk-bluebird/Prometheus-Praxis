public final class CanalBlastRadius {
    private CanalBlastRadius() {}

    private static double clamp01(double value) {
        return Math.max(0.0, Math.min(1.0, value));
    }

    private record Assessment(
        double baseRadiusM,
        double conservativeRadiusM,
        double knowledgeFactor,
        double ecoImpactValue,
        double harmRisk,
        String zone,
        String machineAction
    ) {}

    private static Assessment assess(
        double breachFlowLps,
        double surchargeDurationS,
        double bankSensitivity,
        double distanceM,
        double energyreqJ,
        double deltaVt
    ) {
        if (breachFlowLps <= 0.0 || surchargeDurationS <= 0.0 || distanceM < 0.0 ||
            energyreqJ < 0.0 || deltaVt < 0.0 || bankSensitivity < 0.0 || bankSensitivity > 1.0) {
            throw new IllegalArgumentException(
                "inputs must be non-negative; flow and duration must be positive; sensitivity must be 0..1"
            );
        }

        double baseRadiusM = Math.sqrt(breachFlowLps * surchargeDurationS) / 10.0;
        double conservativeRadiusM = baseRadiusM * (1.0 + bankSensitivity * 1.5);
        double exposure = conservativeRadiusM <= 0.0
            ? 0.0 : clamp01(1.0 - distanceM / conservativeRadiusM);
        double energyLoad = clamp01(energyreqJ / 1_000_000.0);
        double velocityLoad = clamp01(deltaVt / 10.0);
        double harmRisk = clamp01(
            0.60 * exposure + 0.20 * bankSensitivity + 0.10 * energyLoad + 0.10 * velocityLoad
        );
        double knowledgeFactor = clamp01(1.0 - 0.35 * bankSensitivity - 0.25 * energyLoad);
        double ecoImpactValue = clamp01((1.0 - harmRisk) * (0.40 + 0.60 * knowledgeFactor));

        if (harmRisk >= 0.60) {
            return new Assessment(
                baseRadiusM, conservativeRadiusM, knowledgeFactor, ecoImpactValue,
                harmRisk, "EXCLUDE", "NO_ENTRY"
            );
        }
        if (harmRisk > 0.25) {
            return new Assessment(
                baseRadiusM, conservativeRadiusM, knowledgeFactor, ecoImpactValue,
                harmRisk, "CAUTION", "HOLD_FOR_INSPECTION"
            );
        }
        return new Assessment(
            baseRadiusM, conservativeRadiusM, knowledgeFactor, ecoImpactValue,
            harmRisk, "SAFE", "OPERATE_LOW_IMPACT"
        );
    }

    public static void main(String[] args) {
        if (args.length != 6) {
            System.err.println(
                "usage: CanalBlastRadius <breach_flow_lps> <surcharge_duration_s> "
                + "<bank_sensitivity_0_to_1> <distance_m> <energyreqJ> <delta_vt>"
            );
            System.exit(64);
        }

        try {
            Assessment result = assess(
                Double.parseDouble(args[0]),
                Double.parseDouble(args[1]),
                Double.parseDouble(args[2]),
                Double.parseDouble(args[3]),
                Double.parseDouble(args[4]),
                Double.parseDouble(args[5])
            );

            System.out.printf("base_radius_m=%.3f%n", result.baseRadiusM());
            System.out.printf("conservative_radius_m=%.3f%n", result.conservativeRadiusM());
            System.out.printf("knowledge_factor=%.3f%n", result.knowledgeFactor());
            System.out.printf("eco_impact_value=%.3f%n", result.ecoImpactValue());
            System.out.printf("harm_risk=%.3f%n", result.harmRisk());
            System.out.println("zone=" + result.zone());
            System.out.println("machine_action=" + result.machineAction());
        } catch (IllegalArgumentException exception) {
            System.err.println("error: " + exception.getMessage());
            System.exit(65);
        }
    }
}
