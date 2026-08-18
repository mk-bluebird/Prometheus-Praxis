import java.util.ArrayList;
import java.util.List;

public final class OrdinaryKriging {
    private OrdinaryKriging() {}

    private record Observation(double x, double y, double value) {}
    private record Result(double estimate, double variance, double weightSum) {}

    private static double semivariance(double h, double nugget, double sill, double range) {
        return h <= 0.0 ? 0.0 : nugget + sill * (1.0 - Math.exp(-h / range));
    }

    private static double distance(double x1, double y1, double x2, double y2) {
        double dx = x1 - x2;
        double dy = y1 - y2;
        return Math.sqrt(dx * dx + dy * dy);
    }

    private static double[] solve(double[][] matrix, double[] rhs) {
        int n = rhs.length;

        for (int column = 0; column < n; column++) {
            int pivot = column;
            for (int row = column + 1; row < n; row++) {
                if (Math.abs(matrix[row][column]) > Math.abs(matrix[pivot][column])) {
                    pivot = row;
                }
            }

            if (Math.abs(matrix[pivot][column]) <= 1.0e-12) {
                throw new IllegalArgumentException("singular kriging system");
            }

            double[] swapRow = matrix[pivot];
            matrix[pivot] = matrix[column];
            matrix[column] = swapRow;

            double swapValue = rhs[pivot];
            rhs[pivot] = rhs[column];
            rhs[column] = swapValue;

            double divisor = matrix[column][column];
            for (int col = column; col < n; col++) {
                matrix[column][col] /= divisor;
            }
            rhs[column] /= divisor;

            for (int row = 0; row < n; row++) {
                if (row == column) {
                    continue;
                }

                double factor = matrix[row][column];
                for (int col = column; col < n; col++) {
                    matrix[row][col] -= factor * matrix[column][col];
                }
                rhs[row] -= factor * rhs[column];
            }
        }

        return rhs;
    }

    private static Result interpolate(
        List<Observation> observations,
        double targetX,
        double targetY,
        double nugget,
        double sill,
        double range
    ) {
        if (observations.size() < 2 || nugget < 0.0 || sill < 0.0 || range <= 0.0) {
            throw new IllegalArgumentException("invalid kriging inputs");
        }

        int n = observations.size();
        double[][] matrix = new double[n + 1][n + 1];
        double[] rhs = new double[n + 1];

        for (int row = 0; row < n; row++) {
            Observation left = observations.get(row);
            for (int col = 0; col < n; col++) {
                Observation right = observations.get(col);
                matrix[row][col] = semivariance(
                    distance(left.x(), left.y(), right.x(), right.y()),
                    nugget, sill, range
                );
            }
            matrix[row][n] = 1.0;
            matrix[n][row] = 1.0;
            rhs[row] = semivariance(distance(left.x(), left.y(), targetX, targetY), nugget, sill, range);
        }

        rhs[n] = 1.0;
        double[] solution = solve(matrix, rhs);
        double estimate = 0.0;
        double variance = solution[n];
        double weightSum = 0.0;

        for (int index = 0; index < n; index++) {
            estimate += solution[index] * observations.get(index).value();
            variance += solution[index] * rhs[index];
            weightSum += solution[index];
        }

        return new Result(estimate, Math.max(0.0, variance), weightSum);
    }

    public static void main(String[] args) {
        if (args.length < 11 || (args.length - 5) % 3 != 0) {
            System.err.println(
                "usage: OrdinaryKriging <target_x_m> <target_y_m> <nugget> <sill> <range_m> " +
                    "<x_m> <y_m> <value> [<x_m> <y_m> <value> ...]"
            );
            System.exit(64);
        }

        try {
            double targetX = Double.parseDouble(args[0]);
            double targetY = Double.parseDouble(args[1]);
            double nugget = Double.parseDouble(args[2]);
            double sill = Double.parseDouble(args[3]);
            double range = Double.parseDouble(args[4]);

            List<Observation> observations = new ArrayList<>();
            for (int index = 5; index < args.length; index += 3) {
                observations.add(new Observation(
                    Double.parseDouble(args[index]),
                    Double.parseDouble(args[index + 1]),
                    Double.parseDouble(args[index + 2])
                ));
            }

            Result result = interpolate(observations, targetX, targetY, nugget, sill, range);
            System.out.printf("estimate=%.10f%n", result.estimate());
            System.out.printf("kriging_variance=%.10f%n", result.variance());
            System.out.printf("weight_sum=%.10f%n", result.weightSum());
            System.out.println(
                "unbiasedness_constraint=" +
                    (Math.abs(result.weightSum() - 1.0) <= 1.0e-8 ? "SATISFIED" : "FAILED")
            );
        } catch (IllegalArgumentException exception) {
            System.err.println("error: " + exception.getMessage());
            System.exit(65);
        }
    }
}
