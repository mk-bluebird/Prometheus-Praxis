import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

public final class DeltaVtIntegrator {
    private static final double GOVERNANCE_DELTA_VT_LIMIT = 10000.0;

    private DeltaVtIntegrator() {}

    private record AccelerationPoint(double timeSeconds, double accelerationMetersPerSecondSquared) {}

    private static double integrateDeltaVt(List<AccelerationPoint> rawPoints) {
        if (rawPoints.size() < 2) {
            throw new IllegalArgumentException("at least two acceleration points are required");
        }

        List<AccelerationPoint> points = new ArrayList<>(rawPoints);
        points.sort(Comparator.comparingDouble(AccelerationPoint::timeSeconds));

        double deltaVt = 0.0;
        for (int index = 1; index < points.size(); index++) {
            AccelerationPoint prior = points.get(index - 1);
            AccelerationPoint current = points.get(index);

            if (!Double.isFinite(prior.timeSeconds()) ||
                !Double.isFinite(current.timeSeconds()) ||
                !Double.isFinite(prior.accelerationMetersPerSecondSquared()) ||
                !Double.isFinite(current.accelerationMetersPerSecondSquared())) {
                throw new IllegalArgumentException("all samples must be finite");
            }

            double deltaTime = current.timeSeconds() - prior.timeSeconds();
            if (deltaTime <= 0.0) {
                throw new IllegalArgumentException("time values must be strictly increasing");
            }

            deltaVt += 0.5 * (
                prior.accelerationMetersPerSecondSquared() +
                current.accelerationMetersPerSecondSquared()
            ) * deltaTime;
        }
        return deltaVt;
    }

    public static void main(String[] args) {
        if (args.length < 4 || args.length % 2 != 0) {
            System.err.println(
                "usage: DeltaVtIntegrator <time_s> <acceleration_m_s2> [<time_s> <acceleration_m_s2> ...]"
            );
            System.exit(64);
        }

        try {
            List<AccelerationPoint> points = new ArrayList<>();
            for (int index = 0; index < args.length; index += 2) {
                points.add(new AccelerationPoint(
                    Double.parseDouble(args[index]),
                    Double.parseDouble(args[index + 1])
                ));
            }

            double deltaVt = integrateDeltaVt(points);
            System.out.printf("delta_vt_m_per_s=%.8f%n", deltaVt);
            System.out.println(
                "governance_limit_screen=" +
                (Math.abs(deltaVt) <= GOVERNANCE_DELTA_VT_LIMIT ? "WITHIN_10000_INVARIANT" : "EXCEEDS_10000_INVARIANT")
            );
            System.out.println(
                "site_safety_note=Use an equipment-specific limit below the governance ceiling, established by qualified site review."
            );
        } catch (IllegalArgumentException exception) {
            System.err.println("error: " + exception.getMessage());
            System.exit(65);
        }
    }
}
