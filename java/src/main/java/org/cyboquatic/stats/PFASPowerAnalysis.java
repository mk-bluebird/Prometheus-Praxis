// File: java/src/main/java/org/cyboquatic/stats/PFASPowerAnalysis.java
package org.cyboquatic.stats;

import org.apache.commons.math3.distribution.PoissonDistribution;

/**
 * Information-theoretic power analysis for PFAS detection in cyboquatic canals.
 *
 * Goal: compute the minimal telemetry sampling interval (and thus sample count)
 * needed to detect a 10% reduction in PFAS concentration with 99% power under
 * Poisson-noise-dominated measurements, wired to cyboquatic_workload_telemetry.
 *
 * Assumptions:
 *  - PFAS detections per sample follow a Poisson distribution with mean λ
 *    proportional to underlying concentration C.
 *  - We test H0: λ = λ0 vs. H1: λ = λ1 = 0.9 λ0 (10% reduction).
 *  - We collect N samples at interval Δt; total information grows with N.
 *
 * Information-theoretic lower bound:
 *  - Use Kullback-Leibler divergence D_KL(P0 || P1) between Poisson(λ0) and Poisson(λ1):
 *
 *      D_KL = λ0 * (log(λ0 / λ1)) + λ1 - λ0
 *
 *    where log is natural log.
 *
 *  - The asymptotic error exponent for hypothesis testing scales as N * D_KL.
 *    To achieve power 1 - β (here 0.99) at significance α, an approximate bound:
 *
 *      N ≥ (Z_{1-α} + Z_{1-β})^2 / I_eff,
 *
 *    where I_eff is effective information per sample, approximated here by D_KL.
 *
 *    For small α and β, a simpler conservative bound is:
 *
 *      N ≥ c / D_KL,
 *
 *    with c chosen from desired error exponents (e.g., c ≈ 10 for high power).
 *
 *  - Once N is known, sampling interval Δt over horizon T gives:
 *
 *      Δt = T / N.
 */
public final class PFASPowerAnalysis {

    private PFASPowerAnalysis() {}

    public static class Result {
        public final int samplesNeeded;
        public final double klDivergence;
        public final double recommendedIntervalSeconds;

        public Result(int samplesNeeded, double klDivergence, double recommendedIntervalSeconds) {
            this.samplesNeeded = samplesNeeded;
            this.klDivergence = klDivergence;
            this.recommendedIntervalSeconds = recommendedIntervalSeconds;
        }
    }

    /**
     * Compute the minimal number of samples and sampling interval to detect
     * a 10% reduction in PFAS concentration with 99% power over a horizon T seconds.
     *
     * @param lambda0Baseline mean Poisson count at baseline concentration.
     * @param horizonSeconds  total monitoring horizon (e.g., one storm event or season).
     * @return Result containing samplesNeeded, KL divergence, and recommended Δt.
     */
    public static Result computeSamplingInterval(double lambda0Baseline,
                                                 double horizonSeconds) {
        double lambda1 = 0.9 * lambda0Baseline; // 10% reduction

        // Kullback-Leibler divergence D_KL(P0 || P1) for Poisson:
        double kl = lambda0Baseline * Math.log(lambda0Baseline / lambda1)
                    + lambda1 - lambda0Baseline;

        // Conservative constant c to target ~99% power (heuristic).
        double c = 10.0;

        int samples = (int) Math.ceil(c / kl);
        double deltaT = horizonSeconds / samples;

        return new Result(samples, kl, deltaT);
    }

    public static void main(String[] args) {
        double lambda0 = 5.0;         // baseline PFAS count per sample
        double horizonSeconds = 3600; // 1-hour window

        Result r = computeSamplingInterval(lambda0, horizonSeconds);
        System.out.println("Samples needed: " + r.samplesNeeded);
        System.out.println("KL divergence: " + r.klDivergence);
        System.out.println("Recommended sampling interval (s): " + r.recommendedIntervalSeconds);
    }
}
