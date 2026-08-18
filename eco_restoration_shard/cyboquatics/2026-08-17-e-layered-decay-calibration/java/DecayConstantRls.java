public final class DecayConstantRls {
    private DecayConstantRls() {}

    private record State(double kPerDay, double covariance, double referenceFlowM3S) {}
    private record Update(
        State state,
        double phi,
        double y,
        double residual,
        double gain,
        double flowDriftFraction,
        boolean accepted
    ) {}

    private static double clamp(double value, double low, double high) {
        return Math.max(low, Math.min(high, value));
    }

    private static Update update(
        State state,
        double elapsedDays,
        double concentrationMgL,
        double referenceConcentrationMgL,
        double flowM3S,
        double forgettingFactor,
        double covarianceMin,
        double covarianceMax,
        double maximumFlowDriftFraction
    ) {
        if (elapsedDays <= 0.0 || concentrationMgL <= 0.0 || referenceConcentrationMgL <= 0.0 ||
            flowM3S < 0.0 || forgettingFactor <= 0.0 || forgettingFactor > 1.0 ||
            covarianceMin <= 0.0 || covarianceMax < covarianceMin ||
            state.covariance() < covarianceMin || state.covariance() > covarianceMax ||
            maximumFlowDriftFraction < 0.0) {
            throw new IllegalArgumentException("invalid RLS update input");
        }

        double phi = -elapsedDays;
        double y = Math.log(concentrationMgL / referenceConcentrationMgL);
        double referenceFlow = state.referenceFlowM3S() > 0.0 ? state.referenceFlowM3S() : Math.max(flowM3S, 1.0e-12);
        double flowDrift = Math.abs(flowM3S - referenceFlow) / referenceFlow;
        boolean accepted = flowDrift <= maximumFlowDriftFraction;

        double denominator = forgettingFactor + phi * state.covariance() * phi;
        double gain = state.covariance() * phi / denominator;
        double residual = y - phi * state.kPerDay();
        double rawCovariance = (state.covariance() - gain * phi * state.covariance()) / forgettingFactor;
        double nextCovariance = clamp(rawCovariance, covarianceMin, covarianceMax);
        double nextK = accepted ? state.kPerDay() + gain * residual : state.kPerDay();

        return new Update(
            new State(nextK, nextCovariance, referenceFlow),
            phi, y, residual, gain, flowDrift, accepted
        );
    }

    public static void main(String[] args) {
        if (args.length < 11 || (args.length - 8) % 3 != 0) {
            System.err.println(
                "usage: DecayConstantRls <k0_per_day> <P0> <reference_flow_m3_s> <lambda> <Pmin> <Pmax> " +
                "<max_flow_drift_fraction> <C0_mg_l> <elapsed_days> <C_mg_l> <flow_m3_s> [...]"
            );
            System.exit(64);
        }

        try {
            State state = new State(
                Double.parseDouble(args[0]),
                Double.parseDouble(args[1]),
                Double.parseDouble(args[2])
            );
            double lambda = Double.parseDouble(args[3]);
            double pMin = Double.parseDouble(args[4]);
            double pMax = Double.parseDouble(args[5]);
            double maxFlowDrift = Double.parseDouble(args[6]);
            double c0 = Double.parseDouble(args[7]);

            for (int index = 8, step = 0; index < args.length; index += 3, step++) {
                Update result = update(
                    state,
                    Double.parseDouble(args[index]),
                    Double.parseDouble(args[index + 1]),
                    c0,
                    Double.parseDouble(args[index + 2]),
                    lambda,
                    pMin,
                    pMax,
                    maxFlowDrift
                );
                state = result.state();

                System.out.printf(
                    "step=%d accepted=%s k_per_day=%.8f P=%.8f phi=%.8f y=%.8f residual=%.8f gain=%.8f flow_drift=%.8f%n",
                    step,
                    result.accepted(),
                    state.kPerDay(),
                    state.covariance(),
                    result.phi(),
                    result.y(),
                    result.residual(),
                    result.gain(),
                    result.flowDriftFraction()
                );
            }
        } catch (IllegalArgumentException exception) {
            System.err.println("error: " + exception.getMessage());
            System.exit(65);
        }
    }
}
