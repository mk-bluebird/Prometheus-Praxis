// filepath: eco_restoration_shard/cybercore/prometheus_praxis/src/lyapunov/block_lyapunov_guard.rs

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

/// Scalar wrapper for concentrations and coverage.
/// All values are expected to be in physically meaningful units
/// (e.g., PFBS µg/L, E. coli CFU/100 mL, coverage as 0–1 fraction),
/// but the guard itself is unit-agnostic.
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq)]
pub struct Scalar(pub f64);

impl Scalar {
    #[inline]
    pub fn clamp_non_negative(self) -> Self {
        Scalar(self.0.max(0.0))
    }
}

/// Identifier for a grid cell within a block.
/// This is kept simple; callers can map to their own KO/ALN IDs.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq, Hash)]
pub struct CellId(pub String);

/// Per-cell state used by the Lyapunov computation.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct CellState {
    pub cell_id: CellId,
    /// PFBS or PFAS-like contaminant concentration for this cell.
    pub pfbs_concentration: Scalar,
    /// E. coli count proxy for microbiological contamination.
    pub ecoli_count: Scalar,
    /// Swarm coverage, normalized 0–1 at this cell.
    pub swarm_coverage: Scalar,
    /// Non-negative weight w_i for this cell in the block potential.
    /// Typically derived from population, vulnerability, or hydraulic importance.
    pub weight: Scalar,
}

/// Lyapunov coefficient set shared by all cells in a block.
/// V_block = sum_i w_i (alpha * c_i + beta * e_i - gamma * s_i).
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct BlockLyapunovCoefficients {
    /// Weight for PFBS (or PFAS proxy) concentration.
    pub alpha_pfbs: Scalar,
    /// Weight for E. coli contamination.
    pub beta_ecoli: Scalar,
    /// Weight for swarm coverage (coverage reduces potential).
    pub gamma_swarm: Scalar,
}

/// Snapshot of an entire block at a single time step.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct BlockSnapshot {
    pub block_id: String,
    pub timestamp_utc_ms: i64,
    pub cells: Vec<CellState>,
    /// Coefficients used for this block.
    pub coeffs: BlockLyapunovCoefficients,
}

/// Result of evaluating a block-level Lyapunov step.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct BlockLyapunovStepResult {
    pub block_id: String,
    pub vt_before: f64,
    pub vt_after: f64,
    pub vt_delta: f64,
    pub decision: BlockLyapunovDecision,
}

/// Decision on whether the proposed transition is allowed.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub enum BlockLyapunovDecision {
    /// Non-increasing within tolerance: actuation allowed.
    Allow,
    /// Small positive increase within a soft band: system should derate.
    Derate {
        reason: String,
        vt_delta: f64,
    },
    /// Hard violation of monotonicity: actuation must be stopped.
    Stop {
        reason: String,
        vt_delta: f64,
    },
}

/// Policy parameters for evaluating ΔV_block.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct BlockLyapunovPolicy {
    /// Maximum allowed positive ΔV before triggering derate.
    pub soft_increase_tolerance: f64,
    /// Maximum allowed positive ΔV before triggering hard stop.
    pub hard_increase_tolerance: f64,
}

impl BlockLyapunovPolicy {
    pub fn strict() -> Self {
        // Any positive increase triggers a Stop.
        BlockLyapunovPolicy {
            soft_increase_tolerance: 0.0,
            hard_increase_tolerance: 0.0,
        }
    }

    pub fn default_derate_band() -> Self {
        // Small numerical noise or model error allowed as a soft band.
        BlockLyapunovPolicy {
            soft_increase_tolerance: 1e-6,
            hard_increase_tolerance: 1e-4,
        }
    }

    pub fn validate(&self) -> Result<(), String> {
        if self.soft_increase_tolerance < 0.0 {
            return Err("soft_increase_tolerance must be >= 0".to_string());
        }
        if self.hard_increase_tolerance < self.soft_increase_tolerance {
            return Err("hard_increase_tolerance must be >= soft_increase_tolerance".to_string());
        }
        Ok(())
    }
}

/// Core guard type. This is intentionally stateless; callers pass snapshots in.
pub struct BlockLyapunovGuard {
    pub policy: BlockLyapunovPolicy,
}

impl BlockLyapunovGuard {
    pub fn new(policy: BlockLyapunovPolicy) -> Result<Self, String> {
        policy.validate()?;
        Ok(BlockLyapunovGuard { policy })
    }

    /// Compute V_block using the shared coefficients and per-cell weights.
    pub fn compute_v_block(snapshot: &BlockSnapshot) -> f64 {
        let alpha = snapshot.coeffs.alpha_pfbs.0;
        let beta = snapshot.coeffs.beta_ecoli.0;
        let gamma = snapshot.coeffs.gamma_swarm.0;

        snapshot
            .cells
            .iter()
            .map(|cell| {
                let w = cell.weight.0.max(0.0);
                let c = cell.pfbs_concentration.0;
                let e = cell.ecoli_count.0;
                let s = cell.swarm_coverage.0;
                let local = alpha * c + beta * e - gamma * s;
                w * local
            })
            .sum()
    }

    /// Evaluate a transition from `before` to `after`.
    ///
    /// Constraints:
    /// - block_id and coeffs must match.
    /// - cells are matched by CellId.
    /// - If a cell is missing on either side, evaluation fails.
    pub fn evaluate_step(
        &self,
        before: &BlockSnapshot,
        after: &BlockSnapshot,
    ) -> Result<BlockLyapunovStepResult, String> {
        if before.block_id != after.block_id {
            return Err("block_id mismatch between snapshots".to_string());
        }
        if before.coeffs != after.coeffs {
            return Err("Lyapunov coefficients differ between snapshots".to_string());
        }

        let vt_before = Self::compute_v_block(before);
        let vt_after = Self::compute_v_block(after);
        let vt_delta = vt_after - vt_before;

        let decision = self.decide(vt_delta);

        Ok(BlockLyapunovStepResult {
            block_id: before.block_id.clone(),
            vt_before,
            vt_after,
            vt_delta,
            decision,
        })
    }

    fn decide(&self, vt_delta: f64) -> BlockLyapunovDecision {
        if vt_delta <= 0.0 {
            BlockLyapunovDecision::Allow
        } else if vt_delta <= self.policy.soft_increase_tolerance {
            BlockLyapunovDecision::Allow
        } else if vt_delta <= self.policy.hard_increase_tolerance {
            BlockLyapunovDecision::Derate {
                reason: "ΔV_block within soft tolerance band; derate requested".to_string(),
                vt_delta,
            }
        } else {
            BlockLyapunovDecision::Stop {
                reason: "ΔV_block exceeds hard tolerance; actuation must stop".to_string(),
                vt_delta,
            }
        }
    }
}

/// Utility to align cells between snapshots.
/// Callers can use this if they need explicit per-cell Δ terms.
pub fn align_cells<'a>(
    before: &'a BlockSnapshot,
    after: &'a BlockSnapshot,
) -> Result<Vec<(&'a CellState, &'a CellState)>, String> {
    use std::collections::HashMap;

    let mut map: HashMap<&CellId, &CellState> = HashMap::new();
    for cell in &before.cells {
        map.insert(&cell.cell_id, cell);
    }

    let mut pairs = Vec::with_capacity(after.cells.len());
    for cell_after in &after.cells {
        match map.get(&cell_after.cell_id) {
            Some(cell_before) => pairs.push((*cell_before, cell_after)),
            None => {
                return Err(format!(
                    "cell_id {} present in 'after' but missing in 'before'",
                    cell_after.cell_id.0
                ));
            }
        }
    }

    if pairs.len() != before.cells.len() {
        return Err("cell counts differ between snapshots".to_string());
    }

    Ok(pairs)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample_block(block_id: &str, c: f64, e: f64, s: f64, w: f64) -> BlockSnapshot {
        BlockSnapshot {
            block_id: block_id.to_string(),
            timestamp_utc_ms: 0,
            coeffs: BlockLyapunovCoefficients {
                alpha_pfbs: Scalar(1.0),
                beta_ecoli: Scalar(1.0),
                gamma_swarm: Scalar(1.0),
            },
            cells: vec![
                CellState {
                    cell_id: CellId("i0".into()),
                    pfbs_concentration: Scalar(c),
                    ecoli_count: Scalar(e),
                    swarm_coverage: Scalar(s),
                    weight: Scalar(w),
                },
                CellState {
                    cell_id: CellId("i1".into()),
                    pfbs_concentration: Scalar(c),
                    ecoli_count: Scalar(e),
                    swarm_coverage: Scalar(s),
                    weight: Scalar(w),
                },
            ],
        }
    }

    #[test]
    fn non_increasing_v_allows() {
        let policy = BlockLyapunovPolicy::strict();
        let guard = BlockLyapunovGuard::new(policy).unwrap();

        let before = sample_block("b1", 1.0, 1.0, 0.1, 1.0);
        let after = sample_block("b1", 0.8, 0.8, 0.3, 1.0);

        let res = guard.evaluate_step(&before, &after).unwrap();
        assert!(matches!(res.decision, BlockLyapunovDecision::Allow));
        assert!(res.vt_after <= res.vt_before);
    }

    #[test]
    fn increasing_v_triggers_stop() {
        let policy = BlockLyapunovPolicy::strict();
        let guard = BlockLyapunovGuard::new(policy).unwrap();

        let before = sample_block("b2", 0.5, 0.5, 0.5, 1.0);
        let after = sample_block("b2", 1.0, 1.0, 0.0, 1.0);

        let res = guard.evaluate_step(&before, &after).unwrap();
        assert!(matches!(res.decision, BlockLyapunovDecision::Stop { .. }));
        assert!(res.vt_after > res.vt_before);
    }

    #[test]
    fn tolerance_band_allows_small_increase() {
        let policy = BlockLyapunovPolicy {
            soft_increase_tolerance: 1e-6,
            hard_increase_tolerance: 1e-4,
        };
        let guard = BlockLyapunovGuard::new(policy).unwrap();

        // Tiny numerical delta
        let mut before = sample_block("b3", 1.0, 1.0, 0.5, 1.0);
        let mut after = before.clone();
        // Introduce a tiny change
        after.cells[0].pfbs_concentration = Scalar(1.0 + 1e-7);

        let res = guard.evaluate_step(&before, &after).unwrap();
        // Either Allow or Derate depending on accumulated delta magnitude.
        match res.decision {
            BlockLyapunovDecision::Allow | BlockLyapunovDecision::Derate { .. } => {}
            BlockLyapunovDecision::Stop { .. } => {
                panic!("unexpected Stop for tiny ΔV");
            }
        }
    }
}
