// filename: crates/cyboquatic-ecosafety/src/privacy.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

use core::fmt;

/// Scalar for ecosafety math.
pub type Scalar = f64;

/// Minimal per-operator ecosafety statistics that can be shared in masked form.
#[derive(Debug, Clone)]
pub struct LocalRiskStats {
    pub sample_count: u64,
    pub sum_risk: Scalar,
    pub sum_risk_sq: Scalar,
}

impl LocalRiskStats {
    pub fn new(sample_count: u64, sum_risk: Scalar, sum_risk_sq: Scalar) -> Self {
        Self {
            sample_count,
            sum_risk,
            sum_risk_sq,
        }
    }
}

/// Configuration for differential-privacy over ecosafety statistics.
#[derive(Debug, Clone)]
pub struct DpConfig {
    pub epsilon: Scalar,
    pub delta: Scalar,
    pub max_risk: Scalar,
}

/// Interface for Laplace noise generation; caller supplies the RNG.
pub trait LaplaceSampler {
    fn sample_laplace(&mut self, scale: Scalar) -> Scalar;
}

/// Differential-privacy protected global statistics.
#[derive(Debug, Clone)]
pub struct DpGlobalRiskStats {
    pub sample_count: u64,
    pub mean_risk_dp: Scalar,
    pub variance_risk_dp: Scalar,
}

pub fn apply_dp_to_global_stats<S: LaplaceSampler>(
    base: &GlobalRiskStats,
    cfg: &DpConfig,
    sampler: &mut S,
) -> DpGlobalRiskStats {
    let n = base.sample_count.max(1) as Scalar;

    let sensitivity_mean = cfg.max_risk / n;
    let scale_mean = sensitivity_mean / cfg.epsilon.max(1e-9);

    let sensitivity_var = cfg.max_risk * cfg.max_risk / n;
    let scale_var = sensitivity_var / cfg.epsilon.max(1e-9);

    let mean_dp = base.mean_risk + sampler.sample_laplace(scale_mean);
    let var_dp = (base.variance_risk + sampler.sample_laplace(scale_var)).max(0.0);

    DpGlobalRiskStats {
        sample_count: base.sample_count,
        mean_risk_dp: mean_dp,
        variance_risk_dp: var_dp,
    }
}

/// A single additive share of a local statistics triple.
#[derive(Debug, Clone, Copy)]
pub struct RiskShare {
    pub sample_count_share: i128,
    pub sum_risk_share: Scalar,
    pub sum_risk_sq_share: Scalar,
}

/// Generate additive shares for a single operator.
pub fn make_risk_shares(
    local: &LocalRiskStats,
    parties: usize,
    random_scalars: &[(i128, Scalar, Scalar)],
) -> Vec<RiskShare> {
    assert!(parties >= 2, "at least 2 parties required");
    assert!(
        random_scalars.len() + 1 == parties,
        "need parties-1 random tuples"
    );

    let mut shares = Vec::with_capacity(parties);

    let mut acc_sample: i128 = 0;
    let mut acc_sum_risk: Scalar = 0.0;
    let mut acc_sum_risk_sq: Scalar = 0.0;

    for &(c, s, s2) in random_scalars {
        shares.push(RiskShare {
            sample_count_share: c,
            sum_risk_share: s,
            sum_risk_sq_share: s2,
        });
        acc_sample += c;
        acc_sum_risk += s;
        acc_sum_risk_sq += s2;
    }

    let last = RiskShare {
        sample_count_share: local.sample_count as i128 - acc_sample,
        sum_risk_share: local.sum_risk - acc_sum_risk,
        sum_risk_sq_share: local.sum_risk_sq - acc_sum_risk_sq,
    };
    shares.push(last);

    shares
}

/// Aggregated shares at a single aggregator.
#[derive(Debug, Clone, Default)]
pub struct AggregatedShares {
    pub sample_count_sum: i128,
    pub sum_risk_sum: Scalar,
    pub sum_risk_sq_sum: Scalar,
}

impl AggregatedShares {
    pub fn add_share(&mut self, share: RiskShare) {
        self.sample_count_sum += share.sample_count_share;
        self.sum_risk_sum += share.sum_risk_share;
        self.sum_risk_sq_sum += share.sum_risk_sq_share;
    }
}

/// Final aggregated, non-private statistics reconstructed after combining all aggregators.
#[derive(Debug, Clone)]
pub struct GlobalRiskStats {
    pub sample_count: u64,
    pub mean_risk: Scalar,
    pub variance_risk: Scalar,
}

impl fmt::Display for GlobalRiskStats {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "GlobalRiskStats {{ sample_count: {}, mean_risk: {:.6}, variance_risk: {:.6} }}",
            self.sample_count, self.mean_risk, self.variance_risk
        )
    }
}

pub fn reconstruct_global_stats(total: &AggregatedShares) -> Option<GlobalRiskStats> {
    if total.sample_count_sum <= 0 {
        return None;
    }
    let n = total.sample_count_sum as Scalar;
    let mean = total.sum_risk_sum / n;
    let mean_sq = total.sum_risk_sq_sum / n;
    let var = (mean_sq - mean * mean).max(0.0);

    Some(GlobalRiskStats {
        sample_count: total.sample_count_sum as u64,
        mean_risk: mean,
        variance_risk: var,
    })
}
