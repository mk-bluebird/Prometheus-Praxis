import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public final class TemperatureCorrectedBodFit {
    private TemperatureCorrectedBodFit() {}

    private record Observation(double elapsedDays, double temperatureC, double bodMgL, double flowM3PerS) {}
    private record Fit(double k20PerDay, double theta, double sse, int temperatureLevels) {}

    private static double predict(
        double bod0MgL, double k20PerDay, double theta, double elapsedDays, double temperatureC
    ) {
        return bod0MgL * Math.exp(-k20PerDay * Math.pow(theta, temperatureC - 20.0) * elapsedDays);
    }

    private static Fit fit(
        double bod0MgL,
        List<Observation> observations,
        double k20Minimum,
        double k20Maximum,
        int k20Steps,
        double thetaMinimum,
        double thetaMaximum,
        int thetaSteps
    ) {
        if (bod0MgL <= 0.0 || observations.size() < 3 || k20Minimum <= 0.0 ||
            k20Maximum < k20Minimum || thetaMinimum <= 0.0 || thetaMaximum < thetaMinimum ||
            k20Steps < 2 || thetaSteps < 2) {
            throw new IllegalArgumentException("invalid fit configuration");
        }

        Set<Double> temperatures = new HashSet<>();
        for (Observation observation : observations) {
            if (observation.elapsedDays < 0.0 || observation.temperatureC < -5.0 ||
                observation.temperatureC > 60.0 || observation.bodMgL < 0.0 || observation.flowM3PerS < 0.0) {
                throw new IllegalArgumentException("observation outside screening limits");
            }
            temperatures.add(observation.temperatureC);
        }

        double bestK20 = k20Minimum;
        double bestTheta = thetaMinimum;
        double bestSse = Double.POSITIVE_INFINITY;

        for (int kIndex = 0; kIndex < k20Steps; kIndex++) {
            double k20 = k20Minimum + (k20Maximum - k20Minimum) * kIndex / (k20Steps - 1.0);
            for (int thetaIndex = 0; thetaIndex < thetaSteps; thetaIndex++) {
                double theta = thetaMinimum + (thetaMaximum - thetaMinimum) * thetaIndex / (thetaSteps - 1.0);
                double sse = 0.0;

                for (Observation observation : observations) {
                    double residual = observation.bodMgL - predict(
                        bod0MgL, k20, theta, observation.elapsedDays, observation.temperatureC
                    );
                    sse += residual * residual;
                }

                if (sse < bestSse) {
                    bestSse = sse;
                    bestK20 = k20;
                    bestTheta = theta;
                }
            }
        }

        return new Fit(bestK20, bestTheta, bestSse, temperatures.size());
    }

    public static void main(String[] args) {
        if (args.length < 15 || (args.length - 9) % 4 != 0) {
            System.err.println(
                "usage: TemperatureCorrectedBodFit <BOD0_mg_L> <k20_min> <k20_max> <k20_steps> " +
                "<theta_min> <theta_max> <theta_steps> <reserved_1> <reserved_2> " +
                "<time_days> <temperature_C> <BOD_mg_L> <flow_m3_s> [...]"
            );
            System.exit(64);
        }

        try {
            double bod0 = Double.parseDouble(args[0]);
            double k20Minimum = Double.parseDouble(args[1]);
            double k20Maximum = Double.parseDouble(args[2]);
            int k20Steps = Integer.parseInt(args[3]);
            double thetaMinimum = Double.parseDouble(args[4]);
            double thetaMaximum = Double.parseDouble(args[5]);
            int thetaSteps = Integer.parseInt(args[6]);

            List<Observation> observations = new ArrayList<>();
            for (int index = 9; index < args.length; index += 4) {
                observations.add(new Observation(
                    Double.parseDouble(args[index]),
                    Double.parseDouble(args[index + 1]),
                    Double.parseDouble(args[index + 2]),
                    Double.parseDouble(args[index + 3])
                ));
            }

            Fit result = fit(
                bod0, observations, k20Minimum, k20Maximum, k20Steps,
                thetaMinimum, thetaMaximum, thetaSteps
            );

            System.out.printf("k20_per_day=%.8f%n", result.k20PerDay());
            System.out.printf("theta=%.8f%n", result.theta());
            System.out.printf("sum_squared_error=%.8f%n", result.sse());
            System.out.println("temperature_levels=" + result.temperatureLevels());
            System.out.println(
                "identifiability_screen=" +
                (result.temperatureLevels() >= 2 ? "TEMPERATURE_VARIATION_PRESENT" : "ONLY_EFFECTIVE_RATE_IDENTIFIABLE")
            );
        } catch (IllegalArgumentException exception) {
            System.err.println("error: " + exception.getMessage());
            System.exit(65);
        }
    }
}
