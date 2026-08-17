import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

public final class TimeVaryingAlgalGrowth {
    private TimeVaryingAlgalGrowth() {}

    private record ExposurePoint(double timeHours, double concentrationMgL) {}
    private record Result(double integratedGrowth, double controlGrowth, double inhibition, boolean monotonicDecreasing) {}

    private static double clamp(double value, double low, double high) {
        return Math.max(low, Math.min(high, value));
    }

    private static double concentrationAt(List<ExposurePoint> points, double timeHours) {
        if (timeHours <= points.get(0).timeHours()) {
            return points.get(0).concentrationMgL();
        }
        for (int index = 1; index < points.size(); index++) {
            ExposurePoint left = points.get(index - 1);
            ExposurePoint right = points.get(index);
            if (timeHours <= right.timeHours()) {
                double width = right.timeHours() - left.timeHours();
                double fraction = (timeHours - left.timeHours()) / width;
                return left.concentrationMgL() + fraction * (right.concentrationMgL() - left.concentrationMgL());
            }
        }
        return points.get(points.size() - 1).concentrationMgL();
    }

    private static double growthRate(
        double concentrationMgL, double muMaxPerHour, double substrateMgL, double ksMgL, double ec50MgL
    ) {
        double substrateTerm = substrateMgL / (ksMgL + substrateMgL);
        double toxicityTerm = clamp(1.0 - concentrationMgL / ec50MgL, 0.0, 1.0);
        return muMaxPerHour * substrateTerm * toxicityTerm;
    }

    private static Result integrate(
        List<ExposurePoint> points, double muMaxPerHour, double substrateMgL, double ksMgL, double ec50MgL, double stepHours
    ) {
        points.sort(Comparator.comparingDouble(ExposurePoint::timeHours));
        if (points.size() < 2 || muMaxPerHour < 0.0 || substrateMgL < 0.0 || ksMgL < 0.0 || ec50MgL <= 0.0 || stepHours <= 0.0) {
            throw new IllegalArgumentException("invalid parameters or insufficient exposure points");
        }

        boolean monotonicDecreasing = true;
        for (int index = 1; index < points.size(); index++) {
            ExposurePoint previous = points.get(index - 1);
            ExposurePoint current = points.get(index);
            if (current.timeHours() <= previous.timeHours() || current.concentrationMgL() < 0.0) {
                throw new IllegalArgumentException("times must strictly increase and concentrations must be non-negative");
            }
            monotonicDecreasing &= current.concentrationMgL() <= previous.concentrationMgL();
        }

        double start = points.get(0).timeHours();
        double end = points.get(points.size() - 1).timeHours();
        double integral = 0.0;
        double controlRate = muMaxPerHour * substrateMgL / (ksMgL + substrateMgL);

        for (double current = start; current < end;) {
            double next = Math.min(end, current + stepHours);
            double leftRate = growthRate(concentrationAt(points, current), muMaxPerHour, substrateMgL, ksMgL, ec50MgL);
            double rightRate = growthRate(concentrationAt(points, next), muMaxPerHour, substrateMgL, ksMgL, ec50MgL);
            integral += 0.5 * (leftRate + rightRate) * (next - current);
            current = next;
        }

        double controlGrowth = controlRate * (end - start);
        double inhibition = controlGrowth <= 0.0 ? 0.0 : clamp(1.0 - integral / controlGrowth, 0.0, 1.0);
        return new Result(integral, controlGrowth, inhibition, monotonicDecreasing);
    }

    public static void main(String[] args) {
        if (args.length < 9 || ((args.length - 6) % 2 != 0)) {
            System.err.println(
                "usage: TimeVaryingAlgalGrowth <muMax_per_h> <S_mg_L> <Ks_mg_L> <EC50_mg_L> <step_h> "
                + "<time_h> <C_mg_L> [<time_h> <C_mg_L> ...]"
            );
            System.exit(64);
        }

        try {
            double muMax = Double.parseDouble(args[0]);
            double substrate = Double.parseDouble(args[1]);
            double ks = Double.parseDouble(args[2]);
            double ec50 = Double.parseDouble(args[3]);
            double step = Double.parseDouble(args[4]);

            List<ExposurePoint> points = new ArrayList<>();
            for (int index = 5; index < args.length; index += 2) {
                points.add(new ExposurePoint(Double.parseDouble(args[index]), Double.parseDouble(args[index + 1])));
            }

            Result result = integrate(points, muMax, substrate, ks, ec50, step);
            System.out.printf("integrated_growth=%.8f%n", result.integratedGrowth());
            System.out.printf("control_growth=%.8f%n", result.controlGrowth());
            System.out.printf("growth_inhibition=%.8f%n", result.inhibition());
            System.out.println("exposure_monotonic_decreasing=" + result.monotonicDecreasing());
            System.out.println("ec50_interpretation=" +
                (result.monotonicDecreasing()
                    ? "TRAJECTORY_DEPENDENT_NOT_CONSTANT_EXPOSURE_EC50"
                    : "NONMONOTONIC_TRAJECTORY_NOT_CONSTANT_EXPOSURE_EC50")
            );
        } catch (IllegalArgumentException exception) {
            System.err.println("error: " + exception.getMessage());
            System.exit(65);
        }
    }
}
