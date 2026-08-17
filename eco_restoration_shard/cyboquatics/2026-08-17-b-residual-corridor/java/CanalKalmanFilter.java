import java.util.ArrayList;
import java.util.List;

public final class CanalKalmanFilter {
    private CanalKalmanFilter() {}

    private record State(double estimate, double covariance) {}
    private record Result(State state, double residual, double innovationVariance, double normalizedResidual, double gain, boolean updated) {}

    private static Result update(
        State state,
        double processVariance,
        double measurementVariance,
        double h,
        double measurement,
        boolean available
    ) {
        if (processVariance < 0.0 || measurementVariance <= 0.0 || h == 0.0) {
            throw new IllegalArgumentException("require Q >= 0, R > 0, and non-zero H");
        }

        double priorEstimate = state.estimate();
        double priorCovariance = state.covariance() + processVariance;

        if (!available) {
            return new Result(new State(priorEstimate, priorCovariance), Double.NaN, Double.NaN, Double.NaN, 0.0, false);
        }

        double residual = measurement - h * priorEstimate;
        double innovationVariance = h * priorCovariance * h + measurementVariance;
        double gain = priorCovariance * h / innovationVariance;
        double posteriorEstimate = priorEstimate + gain * residual;
        double posteriorCovariance = Math.max(0.0, (1.0 - gain * h) * priorCovariance);
        return new Result(
            new State(posteriorEstimate, posteriorCovariance),
            residual,
            innovationVariance,
            residual / Math.sqrt(innovationVariance),
            gain,
            true
        );
    }

    private static double lagOneAutocorrelation(List<Double> values) {
        if (values.size() < 3) {
            return Double.NaN;
        }

        double mean = values.stream().mapToDouble(Double::doubleValue).average().orElse(Double.NaN);
        double numerator = 0.0;
        double denominator = 0.0;
        for (int index = 0; index < values.size(); index++) {
            double centered = values.get(index) - mean;
            denominator += centered * centered;
            if (index > 0) {
                numerator += centered * (values.get(index - 1) - mean);
            }
        }
        return denominator <= 0.0 ? Double.NaN : numerator / denominator;
    }

    public static void main(String[] args) {
        if (args.length < 8 || (args.length - 6) % 2 != 0) {
            System.err.println(
                "usage: CanalKalmanFilter <x0> <P0> <Q> <R> <H> <measurement_or_zero> <available_0_or_1> "
                + "[<measurement_or_zero> <available_0_or_1> ...]"
            );
            System.exit(64);
        }

        try {
            State state = new State(Double.parseDouble(args[0]), Double.parseDouble(args[1]));
            double processVariance = Double.parseDouble(args[2]);
            double measurementVariance = Double.parseDouble(args[3]);
            double h = Double.parseDouble(args[4]);

            if (state.covariance() < 0.0) {
                throw new IllegalArgumentException("P0 must be non-negative");
            }

            List<Double> innovations = new ArrayList<>();
            for (int index = 5, step = 0; index < args.length; index += 2, step++) {
                boolean available = Integer.parseInt(args[index + 1]) == 1;
                double measurement = available ? Double.parseDouble(args[index]) : 0.0;
                Result result = update(state, processVariance, measurementVariance, h, measurement, available);
                state = result.state();

                System.out.printf(
                    "step=%d updated=%d estimate=%.8f covariance=%.8f gain=%.8f%n",
                    step, result.updated() ? 1 : 0, state.estimate(), state.covariance(), result.gain()
                );

                if (result.updated()) {
                    innovations.add(result.normalizedResidual());
                    System.out.printf(
                        "residual=%.8f S=%.8f normalized_residual=%.8f%n",
                        result.residual(), result.innovationVariance(), result.normalizedResidual()
                    );
                }
            }

            double lagOne = lagOneAutocorrelation(innovations);
            System.out.println("available_innovation_count=" + innovations.size());
            System.out.println(
                "lag1_normalized_residual_autocorrelation=" +
                (Double.isFinite(lagOne) ? String.format("%.8f", lagOne) : "UNAVAILABLE")
            );
            System.out.println(
                "whiteness_screen=" +
                (Double.isFinite(lagOne) && Math.abs(lagOne) <= 0.20
                    ? "PASS_SCREEN_ONLY" : "REVIEW_MODEL_OR_COLLECT_DATA")
            );
        } catch (IllegalArgumentException exception) {
            System.err.println("error: " + exception.getMessage());
            System.exit(65);
        }
    }
}
