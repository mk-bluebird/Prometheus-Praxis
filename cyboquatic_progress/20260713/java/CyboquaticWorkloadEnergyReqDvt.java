// filename: eco_restoration_shard/cyboquatic_progress/20260713/java/CyboquaticWorkloadEnergyReqDvt.java
// domain: (d) Cyboquatic workload in Java (non-actuating diagnostics)
// purpose: Mirror the C++ workload risk kernel and provide a Java API for telemetry services.

package org.cyboquatic.progress;

public final class CyboquaticWorkloadEnergyReqDvt {

    private static final double W_ENERGY = 0.8;
    private static final double W_HYDRAULIC = 1.0;
    private static final double W_UNCERTAINTY = 0.6;

    private static final double ENERGY_TAILWIND_SAFE_RATIO = 1.2;
    private static final double ENERGY_MIN_RATIO = 0.0;
    private static final double ENERGY_MAX_RATIO = 2.5;

    public static final class WorkloadRiskVector {
        public final double renergy;
        public final double rhydraulic;
        public final double runcertainty;

        public WorkloadRiskVector(double renergy, double rhydraulic, double runcertainty) {
            this.renergy = clamp01(renergy);
            this.rhydraulic = clamp01(rhydraulic);
            this.runcertainty = clamp01(runcertainty);
        }

        private static double clamp01(double x) {
            if (x < 0.0) return 0.0;
            if (x > 1.0) return 1.0;
            return x;
        }

        public double residual() {
            return W_ENERGY * renergy * renergy
                 + W_HYDRAULIC * rhydraulic * rhydraulic
                 + W_UNCERTAINTY * runcertainty * runcertainty;
        }
    }

    public static final class WorkloadKer {
        public final double vt;
        public final double deltaVt;
        public final double k;
        public final double e;
        public final double r;

        public WorkloadKer(double vt, double deltaVt, double k, double e, double r) {
            this.vt = vt;
            this.deltaVt = deltaVt;
            this.k = k;
            this.e = e;
            this.r = r;
        }
    }

    public static WorkloadRiskVector normalizeRisk(double energyReqJ,
                                                   double energySurplusJ,
                                                   double hydraulicRisk,
                                                   double uncertaintyRisk) {
        double ratio;
        if (energyReqJ <= 0.0) {
            ratio = ENERGY_MAX_RATIO;
        } else {
            ratio = energySurplusJ / energyReqJ;
        }

        double renergyRaw;
        if (ratio >= ENERGY_TAILWIND_SAFE_RATIO) {
            renergyRaw = 0.0;
        } else if (ratio <= ENERGY_MIN_RATIO) {
            renergyRaw = 1.0;
        } else {
            double bounded = ratio;
            if (bounded > ENERGY_MAX_RATIO) {
                bounded = ENERGY_MAX_RATIO;
            }
            double span = ENERGY_TAILWIND_SAFE_RATIO - ENERGY_MIN_RATIO;
            double rel = (bounded - ENERGY_MIN_RATIO) / span;
            renergyRaw = 1.0 - rel;
            if (renergyRaw < 0.0) renergyRaw = 0.0;
            if (renergyRaw > 1.0) renergyRaw = 1.0;
        }

        return new WorkloadRiskVector(renergyRaw, hydraulicRisk, uncertaintyRisk);
    }

    public static WorkloadKer computeKer(WorkloadRiskVector risk, double vtBefore) {
        double vtBeforeClamped = vtBefore < 0.0 ? 0.0 : vtBefore;
        double vtAfter = risk.residual();
        double deltaVt = vtAfter - vtBeforeClamped;

        double maxR = risk.renergy;
        if (risk.rhydraulic > maxR) maxR = risk.rhydraulic;
        if (risk.runcertainty > maxR) maxR = risk.runcertainty;

        double k = 0.95 - 0.4 * maxR;
        if (deltaVt > 0.0) {
            k -= 0.25;
        }
        if (k < 0.0) k = 0.0;
        if (k > 1.0) k = 1.0;

        double e = 0.95 - vtAfter;
        if (deltaVt > 0.0) {
            e -= 0.3;
        }
        if (e < 0.0) e = 0.0;
        if (e > 1.0) e = 1.0;

        double r = vtAfter;
        if (deltaVt > 0.0) {
            r += deltaVt;
        }
        if (r < 0.0) r = 0.0;
        if (r > 1.0) r = 1.0;

        return new WorkloadKer(vtAfter, deltaVt, k, e, r);
    }

    // Example usage hook for telemetry services, non-actuating:
    public static void main(String[] args) {
        if (args.length != 5) {
            System.err.println("Usage: CyboquaticWorkloadEnergyReqDvt <energyReqJ> <energySurplusJ> <hydraulicRisk> <uncertaintyRisk> <vtBefore>");
            System.exit(1);
        }

        double energyReq = Double.parseDouble(args[0]);
        double energySurplus = Double.parseDouble(args[1]);
        double hydraulicRisk = Double.parseDouble(args[2]);
        double uncertaintyRisk = Double.parseDouble(args[3]);
        double vtBefore = Double.parseDouble(args[4]);

        WorkloadRiskVector risk = normalizeRisk(energyReq, energySurplus, hydraulicRisk, uncertaintyRisk);
        WorkloadKer ker = computeKer(risk, vtBefore);

        System.out.println("renergy=" + risk.renergy
                + " rhydraulic=" + risk.rhydraulic
                + " runcertainty=" + risk.runcertainty);
        System.out.println("vt=" + ker.vt
                + " deltaVt=" + ker.deltaVt
                + " K=" + ker.k
                + " E=" + ker.e
                + " R=" + ker.r);
    }
}
