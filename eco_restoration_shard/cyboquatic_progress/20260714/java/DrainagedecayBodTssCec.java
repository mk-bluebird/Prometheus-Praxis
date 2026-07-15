// File: eco_restoration_shard/cyboquatic_progress/20260714/java/DrainagedecayBodTssCec.java
// Domain (e): drainagedecay frames (BOD, TSS, CEC) for cyboquatic machinery.
// Plain Java 11+ compatible, with no external dependencies.

package cyboquatic.drainagedecay;

public final class DrainagedecayBodTssCec {

    public static final class DrainageState {
        public final double bodMgL;
        public final double tssMgL;
        public final double cecCmolKg;
        public final double temperatureC;
        public final double flowLps;

        public DrainageState(
                double bodMgL,
                double tssMgL,
                double cecCmolKg,
                double temperatureC,
                double flowLps
        ) {
            this.bodMgL = bodMgL;
            this.tssMgL = tssMgL;
            this.cecCmolKg = cecCmolKg;
            this.temperatureC = temperatureC;
            this.flowLps = flowLps;
        }

        public DrainageState copy(
                Double bodMgLOverride,
                Double tssMgLOverride
        ) {
            double bodValue = bodMgLOverride != null ? bodMgLOverride : this.bodMgL;
            double tssValue = tssMgLOverride != null ? tssMgLOverride : this.tssMgL;
            return new DrainageState(
                    bodValue,
                    tssValue,
                    this.cecCmolKg,
                    this.temperatureC,
                    this.flowLps
            );
        }

        @Override
        public String toString() {
            return "DrainageState{" +
                    "bodMgL=" + bodMgL +
                    ", tssMgL=" + tssMgL +
                    ", cecCmolKg=" + cecCmolKg +
                    ", temperatureC=" + temperatureC +
                    ", flowLps=" + flowLps +
                    '}';
        }
    }

    public static final class DecayParameters {
        public final double kBodPerDay;
        public final double kTssPerDay;
        public final double theta;
        public final double refTempC;

        public DecayParameters(
                double kBodPerDay,
                double kTssPerDay,
                double theta,
                double refTempC
        ) {
            this.kBodPerDay = kBodPerDay;
            this.kTssPerDay = kTssPerDay;
            this.theta = theta;
            this.refTempC = refTempC;
        }
    }

    public static DrainageState step(
            DrainageState state,
            DecayParameters params,
            double dtHours
    ) {
        if (dtHours < 0.0) {
            throw new IllegalArgumentException("dtHours must be non-negative");
        }

        double tempFactor = temperatureFactor(
                params.theta,
                params.refTempC,
                state.temperatureC
        );

        double kBodPerHour = params.kBodPerDay / 24.0 * tempFactor;
        double kTssPerHour = params.kTssPerDay / 24.0 * tempFactor;

        double bodNext = firstOrderDecay(state.bodMgL, kBodPerHour, dtHours);
        double tssNext = firstOrderDecay(state.tssMgL, kTssPerHour, dtHours);

        double bodClamped = Math.max(0.0, bodNext);
        double tssClamped = Math.max(0.0, tssNext);

        return state.copy(bodClamped, tssClamped);
    }

    public static double oxygenDemandMgPerSec(DrainageState state) {
        double bodNonNegative = Math.max(0.0, state.bodMgL);
        double flowNonNegative = Math.max(0.0, state.flowLps);
        return bodNonNegative * flowNonNegative / 1000.0;
    }

    private static double firstOrderDecay(
            double initial,
            double kPerHour,
            double dtHours
    ) {
        if (initial <= 0.0) return 0.0;
        if (kPerHour <= 0.0 || dtHours == 0.0) return initial;
        double exponent = -kPerHour * dtHours;
        return initial * Math.exp(exponent);
    }

    private static double temperatureFactor(
            double theta,
            double refTempC,
            double currentTempC
    ) {
        double delta = currentTempC - refTempC;
        return Math.exp(Math.log(theta) * (delta / 10.0));
    }

    public static void main(String[] args) {
        DrainageState initial = new DrainageState(
                35.0,
                60.0,
                20.0,
                21.0,
                4.0
        );

        DecayParameters params = new DecayParameters(
                0.18,
                0.07,
                1.05,
                20.0
        );

        double dtHours = 4.0;

        DrainageState next = step(initial, params, dtHours);
        double oxygenDemand = oxygenDemandMgPerSec(next);

        System.out.println("Initial: " + initial);
        System.out.println("Next after " + dtHours + " h: " + next);
        System.out.println("Oxygen demand (mg O2/s): " + oxygenDemand);
    }
}
