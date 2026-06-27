// filepath: eco_restoration_shard/cybercore/prometheus_praxis/src/lyapunov/pfbs_coverage_cbf_guard.rs

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

use crate::lyapunov::block_lyapunov_guard::{BlockSnapshot, Scalar};

/// Parameters for a PFBS + swarm coverage control barrier function.
///
/// We define a per-cell safety score:
///   h_i = w_cov * s_i - w_pfbs * c_i
/// and a block score:
///   H_block = sum_i h_i.
/// The guard enforces H_block(t+1) >= H_block(t) - eps, with eps >= 0.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct PfbsCoverageCbfParams {
    /// Weight for swarm coverage (positive contribution to safety).
    pub w_cov: Scalar,
    /// Weight for PFBS concentration (negative contribution).
    pub w_pfbs: Scalar,
    /// Allowed small decrease in H_block due to noise / model error.
    pub epsilon_decrease: f64,
}

/// Result of evaluating the CBF over a step.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct PfbsCoverageCbfStep {
    pub block_id: String,
    pub h_before: f64,
    pub h_after: f64,
    pub h_delta: f64,
    pub decision: CbfDecision,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub enum CbfDecision {
    /// Barrier preserved or improved.
    Allow,
    /// Slight degradation within epsilon: advise derate.
    Derate {
        reason: String,
        h_delta: f64,
    },
    /// Barrier violated beyond epsilon: stop.
    Stop {
        reason: String,
        h_delta: f64,
    },
}

impl PfbsCoverageCbfParams {
    pub fn validate(&self) -> Result<(), String> {
        if self.w_cov.0 <= 0.0 {
            return Err("w_cov must be > 0".to_string());
        }
        if self.w_pfbs.0 <= 0.0 {
            return Err("w_pfbs must be > 0".to_string());
        }
        if self.epsilon_decrease < 0.0 {
            return Err("epsilon_decrease must be >= 0".to_string());
        }
        Ok(())
    }
}

/// Stateless CBF evaluator.
pub struct PfbsCoverageCbfGuard {
    params: PfbsCoverageCbfParams,
}

impl PfbsCoverageCbfGuard {
    pub fn new(params: PfbsCoverageCbfParams) -> Result<Self, String> {
        params.validate()?;
        Ok(PfbsCoverageCbfGuard { params })
    }

    /// Compute H_block for a given snapshot.
    pub fn compute_h_block(&self, snapshot: &BlockSnapshot) -> f64 {
        let w_cov = self.params.w_cov.0;
        let w_pfbs = self.params.w_pfbs.0;

        snapshot
            .cells
            .iter()
            .map(|cell| {
                let s = cell.swarm_coverage.0;
                let c = cell.pfbs_concentration.0;
                let h_i = w_cov * s - w_pfbs * c;
                h_i
            })
            .sum()
    }

    /// Evaluate transition against the barrier constraint:
    /// H_after >= H_before - epsilon.
    pub fn evaluate_step(
        &self,
        before: &BlockSnapshot,
        after: &BlockSnapshot,
    ) -> Result<PfbsCoverageCbfStep, String> {
        if before.block_id != after.block_id {
            return Err("block_id mismatch between snapshots".to_string());
        }

        let h_before = self.compute_h_block(before);
        let h_after = self.compute_h_block(after);
        let h_delta = h_after - h_before;

        let decision = if h_delta >= -self.params.epsilon_decrease {
            CbfDecision::Allow
        } else if h_delta >= -2.0 * self.params.epsilon_decrease {
            CbfDecision::Derate {
                reason: "CBF decreased beyond epsilon; derate FOG and local actuation".to_string(),
                h_delta,
            }
        } else {
            CbfDecision::Stop {
                reason: "CBF violated severely; stop PFBS‑affecting and coverage‑reducing routing".to_string(),
                h_delta,
            }
        };

        Ok(PfbsCoverageCbfStep {
            block_id: before.block_id.clone(),
            h_before,
            h_after,
            h_delta,
            decision,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lyapunov::block_lyapunov_guard::{BlockLyapunovCoefficients, CellId, CellState};

    fn mk_snapshot(block_id: &str, c0: f64, s0: f64, c1: f64, s1: f64) -> BlockSnapshot {
        BlockSnapshot {
            block_id: block_id.to_string(),
            timestamp_utc_ms: 0,
            coeffs: BlockLyapunovCoefficients {
                alpha_pfbs: Scalar(1.0),
                beta_ecoli: Scalar(0.0),
                gamma_swarm: Scalar(0.0),
            },
            cells: vec![
                CellState {
                    cell_id: CellId("c0".into()),
                    pfbs_concentration: Scalar(c0),
                    ecoli_count: Scalar(0.0),
                    swarm_coverage: Scalar(s0),
                    weight: Scalar(1.0),
                },
                CellState {
                    cell_id: CellId("c1".into()),
                    pfbs_concentration: Scalar(c1),
                    ecoli_count: Scalar(0.0),
                    swarm_coverage: Scalar(s1),
                    weight: Scalar(1.0),
                },
            ],
        }
    }

    #[test]
    fn cbf_allows_improvement() {
        let params = PfbsCoverageCbfParams {
            w_cov: Scalar(1.0),
            w_pfbs: Scalar(1.0),
            epsilon_decrease: 1e-6,
        };
        let guard = PfbsCoverageCbfGuard::new(params).unwrap();

        let before = mk_snapshot("b1", 1.0, 0.1, 1.0, 0.1);
        let after = mk_snapshot("b1", 0.8, 0.3, 0.8, 0.3);

        let res = guard.evaluate_step(&before, &after).unwrap();
        assert!(matches!(res.decision, CbfDecision::Allow));
        assert!(res.h_after >= res.h_before - 1e-6);
    }

    #[test]
    fn cbf_triggers_stop_on_large_drop() {
        let params = PfbsCoverageCbfParams {
            w_cov: Scalar(1.0),
            w_pfbs: Scalar(1.0),
            epsilon_decrease: 0.01,
        };
        let guard = PfbsCoverageCbfGuard::new(params).unwrap();

        let before = mk_snapshot("b2", 0.5, 0.5, 0.5, 0.5);
        let after = mk_snapshot("b2", 1.5, 0.0, 1.5, 0.0);

        let res = guard.evaluate_step(&before, &after).unwrap();
        assert!(matches!(res.decision, CbfDecision::Stop { .. }));
        assert!(res.h_after < res.h_before - params.epsilon_decrease);
    }
}
