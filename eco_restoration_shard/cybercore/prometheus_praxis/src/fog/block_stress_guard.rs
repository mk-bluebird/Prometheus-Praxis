// filepath: eco_restoration_shard/cybercore/prometheus_praxis/src/fog/block_stress_guard.rs

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

use crate::lyapunov::block_lyapunov_guard::{
    BlockLyapunovDecision, BlockLyapunovGuard, BlockLyapunovPolicy, BlockSnapshot,
};
use crate::lyapunov::pfbs_coverage_cbf_guard::{
    CbfDecision, PfbsCoverageCbfGuard, PfbsCoverageCbfParams,
};

/// High-level decision for FOG-layer routing with respect to block stress.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub enum FogBlockStressDecision {
    /// Both Lyapunov and CBF checks are satisfied.
    Allow,
    /// One or more checks suggests derating additional PFBS / coverage moves.
    Derate {
        reason: String,
    },
    /// Hard violation: routing that increases block stress must be stopped.
    Stop {
        reason: String,
    },
}

/// Configuration tying together Lyapunov and CBF parameters.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct FogBlockStressConfig {
    pub lyapunov_policy: BlockLyapunovPolicy,
    pub cbf_params: PfbsCoverageCbfParams,
}

impl FogBlockStressConfig {
    pub fn validate(&self) -> Result<(), String> {
        self.lyapunov_policy.validate()?;
        self.cbf_params.validate()?;
        Ok(())
    }
}

/// Stateless evaluator that FOG routers can call before dispatching PFBS / coverage workloads.
pub struct FogBlockStressGuard {
    lyap_guard: BlockLyapunovGuard,
    cbf_guard: PfbsCoverageCbfGuard,
}

impl FogBlockStressGuard {
    pub fn new(cfg: FogBlockStressConfig) -> Result<Self, String> {
        cfg.validate()?;
        let lyap_guard = BlockLyapunovGuard::new(cfg.lyapunov_policy)?;
        let cbf_guard = PfbsCoverageCbfGuard::new(cfg.cbf_params)?;
        Ok(FogBlockStressGuard {
            lyap_guard,
            cbf_guard,
        })
    }

    /// Combine Lyapunov and CBF decisions into a single FOG-level routing verdict.
    pub fn decide(
        &self,
        before: &BlockSnapshot,
        after: &BlockSnapshot,
    ) -> Result<FogBlockStressDecision, String> {
        let lyap_step = self.lyap_guard.evaluate_step(before, after)?;
        let cbf_step = self.cbf_guard.evaluate_step(before, after)?;

        let mut reasons: Vec<String> = Vec::new();
        let mut hard_stop = false;
        let mut soft_derate = false;

        match lyap_step.decision {
            BlockLyapunovDecision::Allow => {}
            BlockLyapunovDecision::Derate { ref reason, .. } => {
                soft_derate = true;
                reasons.push(format!("Lyapunov derate: {}", reason));
            }
            BlockLyapunovDecision::Stop { ref reason, .. } => {
                hard_stop = true;
                reasons.push(format!("Lyapunov stop: {}", reason));
            }
        }

        match cbf_step.decision {
            CbfDecision::Allow => {}
            CbfDecision::Derate { ref reason, .. } => {
                soft_derate = true;
                reasons.push(format!("CBF derate: {}", reason));
            }
            CbfDecision::Stop { ref reason, .. } => {
                hard_stop = true;
                reasons.push(format!("CBF stop: {}", reason));
            }
        }

        if hard_stop {
            Ok(FogBlockStressDecision::Stop {
                reason: reasons.join(" | "),
            })
        } else if soft_derate {
            Ok(FogBlockStressDecision::Derate {
                reason: reasons.join(" | "),
            })
        } else {
            Ok(FogBlockStressDecision::Allow)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lyapunov::block_lyapunov_guard::{
        BlockLyapunovCoefficients, CellId, CellState, Scalar,
    };

    fn mk_snapshot(block_id: &str, vt_scale: f64, c: f64, e: f64, s: f64) -> BlockSnapshot {
        BlockSnapshot {
            block_id: block_id.to_string(),
            timestamp_utc_ms: 0,
            coeffs: BlockLyapunovCoefficients {
                alpha_pfbs: Scalar(vt_scale),
                beta_ecoli: Scalar(vt_scale),
                gamma_swarm: Scalar(vt_scale),
            },
            cells: vec![
                CellState {
                    cell_id: CellId("i0".into()),
                    pfbs_concentration: Scalar(c),
                    ecoli_count: Scalar(e),
                    swarm_coverage: Scalar(s),
                    weight: Scalar(1.0),
                },
                CellState {
                    cell_id: CellId("i1".into()),
                    pfbs_concentration: Scalar(c),
                    ecoli_count: Scalar(e),
                    swarm_coverage: Scalar(s),
                    weight: Scalar(1.0),
                },
            ],
        }
    }

    #[test]
    fn fog_guard_allows_safe_step() {
        let cfg = FogBlockStressConfig {
            lyapunov_policy: BlockLyapunovPolicy::default_derate_band(),
            cbf_params: PfbsCoverageCbfParams {
                w_cov: Scalar(1.0),
                w_pfbs: Scalar(1.0),
                epsilon_decrease: 1e-6,
            },
        };
        let guard = FogBlockStressGuard::new(cfg).unwrap();

        let before = mk_snapshot("b1", 1.0, 1.0, 1.0, 0.1);
        let after = mk_snapshot("b1", 1.0, 0.8, 0.8, 0.3);

        let decision = guard.decide(&before, &after).unwrap();
        assert!(matches!(decision, FogBlockStressDecision::Allow));
    }

    #[test]
    fn fog_guard_stops_on_combined_violation() {
        let cfg = FogBlockStressConfig {
            lyapunov_policy: BlockLyapunovPolicy::strict(),
            cbf_params: PfbsCoverageCbfParams {
                w_cov: Scalar(1.0),
                w_pfbs: Scalar(1.0),
                epsilon_decrease: 0.0,
            },
        };
        let guard = FogBlockStressGuard::new(cfg).unwrap();

        // Higher PFBS, lower coverage.
        let before = mk_snapshot("b2", 1.0, 0.5, 0.5, 0.5);
        let after = mk_snapshot("b2", 1.0, 1.5, 1.5, 0.0);

        let decision = guard.decide(&before, &after).unwrap();
        assert!(matches!(decision, FogBlockStressDecision::Stop { .. }));
    }
}
