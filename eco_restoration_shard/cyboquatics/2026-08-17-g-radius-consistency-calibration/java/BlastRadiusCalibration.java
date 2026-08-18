public final class BlastRadiusCalibration {
    private BlastRadiusCalibration() {}

    private record Observation(
        double dischargeM3S,
        double durationS,
        double bankSensitivity,
        double observedRadiusM,
        double coefficientC
    ) {}

    private record Calibration(
        double multiplier,
        double sumSquaredError,
        double meanAbsoluteError,
        int observationCount
    ) {}

    private static Calibration calibrate(Observation[] observations) {
        if (observations.length < 2) {
            throw new IllegalArgumentException("at least two field-surveyed observations are required");
        }

        double numerator = 0.0;
        double denominator = 0.0;

        for (Observation observation : observations) {
            if (observation.dischargeM3S() <= 0.0 || observation.durationS() <= 0.0 ||
                observation.bankSensitivity() < 0.0 || observation.bankSensitivity() > 1.0 ||
                observation.observedRadiusM() < 0.0 || observation.coefficientC() <= 0.0) {
                throw new IllegalArgumentException("observation outside calibration domain");
            }

            double baseRadius = observation.coefficientC() *
                Math.sqrt(observation.dischargeM3S() * observation.durationS());
            double x = baseRadius * observation.bankSensitivity();
            double y = observation.observedRadiusM() - baseRadius;

            numerator += x * y;
            denominator += x * x;
        }

        double unconstrained = denominator <= 0.0 ? 0.0 : numerator / denominator;
        double multiplier = Math.max(0.0, Math.min(1.5, unconstrained));
        double sse = 0.0;
        double absoluteError = 0.0;

        for (Observation observation : observations) {
            double baseRadius = observation.coefficientC() *
                Math.sqrt(observation.dischargeM3S() * observation.durationS());
            double predictedRadius = baseRadius * (1.0 + multiplier * observation.bankSensitivity());
            double residual = observation.observedRadiusM() - predictedRadius;
            sse += residual * residual;
            absoluteError += Math.abs(residual);
        }

        return new Calibration(multiplier, sse, absoluteError / observations.length, observations.length);
    }

    public static void main(String[] args) {
        if (args.length < 10 || args.length % 5 != 0) {
            System.err.println(
                "usage: BlastRadiusCalibration <Q_m3_s> <T_s> <S_b_0_to_1> <observed_radius_m> <c> [...]"
            );
            System.exit(64);
        }

        try {
            Observation[] observations = new Observation[args.length / 5];
            for (int index = 0, observationIndex = 0; index < args.length; index += 5, observationIndex++) {
                observations[observationIndex] = new Observation(
                    Double.parseDouble(args[index]),
                    Double.parseDouble(args[index + 1]),
                    Double.parseDouble(args[index + 2]),
                    Double.parseDouble(args[index + 3]),
                    Double.parseDouble(args[index + 4])
                );
            }

            Calibration result = calibrate(observations);
            System.out.printf("bank_sensitivity_multiplier=%.10f%n", result.multiplier());
            System.out.printf("sum_squared_error=%.10f%n", result.sumSquaredError());
            System.out.printf("mean_absolute_error_m=%.10f%n", result.meanAbsoluteError());
            System.out.println("observation_count=" + result.observationCount());
            System.out.println(
                "calibration_status=" +
                (result.multiplier() >= 1.5 ? "AT_CONSERVATIVE_CAP_REVIEW_REQUIRED" : "SCREENING_CALIBRATION_COMPLETE")
            );
        } catch (IllegalArgumentException exception) {
            System.err.println("error: " + exception.getMessage());
            System.exit(65);
        }
    }
}
