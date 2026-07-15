//! ker-intelligence-scheduler
//!
//! Role: Pure workload intelligence scheduler maximizing
//! I = -Vdot_total / P under monotone corridor tightening.
//!
//! This crate is non-actuating and deterministic. It only chooses
//! normalized workload fractions and corridor band updates, which
//! must be applied by downstream guard crates.

#![forbid(unsafe_code)]

use anyhow::Result;
use nalgebra::{DVector};
use ordered_float::OrderedFloat;
use serde::{Deserialize, Serialize};

/// Execution lane – matches your RESEARCH / PILOT / PRODUCTION semantics.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
pub enum Lane {
    Research,
    Pilot,
    Production,
}

/// KER tier for a workload – higher tier implies stricter tightening.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
pub enum KerTier {
    Tier0,
    Tier1,
    Tier2,
    Tier3,
}

/// Per-plane residuals – e.g. Tsafe, RoH, Lyapunov, aliasing.
/// Stored as a fixed-size vector for convex band metrics.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PlaneResiduals {
    /// Residuals r_i ≥ 0 for each governed plane (Tsafe, RoH, Lyapunov, etc.).
    pub residuals: Vec<f64>,
}

/// A single workload profile, computed by domain guard stacks.
///
/// ΔV_w: expected Lyapunov drop if workload w executes at full rate.
/// P_w: power draw.
/// lane: RESEARCH / PILOT / PRODUCTION.
/// ker_tier: KER-driven tightening tier.
/// plane_residuals: residuals to corridor walls, used to steer band tightening.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct WorkloadProfile {
    pub id: String,
    pub delta_v: f64,
    pub power: f64,
    pub lane: Lane,
    pub ker_tier: KerTier,
    pub plane_residuals: PlaneResiduals,
}

/// Intelligence score for a chosen allocation – scalar objective.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct IntelligenceScore {
    pub vdot_total: f64,
    pub total_power: f64,
    pub i_scalar: f64,
}

/// Soft/hard band parameters for a single corridor.
///
/// soft_start: normalized coordinate where soft band begins.
/// hard_ceiling: normalized coordinate of hard ceiling.
/// Both are in [0, 1], with soft_start < hard_ceiling.
/// Monotone non-loosening: any update must satisfy
/// soft_start_new ≥ soft_start_old, hard_ceiling_new ≤ hard_ceiling_old.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CorridorBands {
    pub soft_start: f64,
    pub hard_ceiling: f64,
}

/// Corridor band update proposal – produced by this crate, applied elsewhere.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CorridorBandUpdate {
    pub corridor_id: String,
    pub soft_start_new: f64,
    pub hard_ceiling_new: f64,
}

/// A convex band-tightening problem instance.
///
/// workloads: profiles for candidate workloads.
/// bands: current bands for each governed corridor.
/// lambda_band: weight for band-tightness versus intelligence.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BandTighteningProblem {
    pub workloads: Vec<WorkloadProfile>,
    pub bands: Vec<(String, CorridorBands)>,
    pub lambda_band: f64,
}

/// Chosen scheduler result:
/// - x: convex combination over workloads.
/// - intelligence: scalar objective.
/// - band_updates: monotone corridor band updates.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SchedulerResult {
    pub allocation: Vec<(String, f64)>,
    pub intelligence: IntelligenceScore,
    pub band_updates: Vec<CorridorBandUpdate>,
}

/// Compute -Vdot_total / P for a convex allocation x over workloads.
///
/// x_w are non-negative weights with ∑_w x_w = 1.
/// Vdot_total = Σ_w x_w * ΔV_w, total_power = Σ_w x_w * P_w.
/// Intelligence I = -Vdot_total / total_power.
fn intelligence_for_allocation(workloads: &[WorkloadProfile], x: &[f64]) -> IntelligenceScore {
    assert_eq!(workloads.len(), x.len());

    let mut vdot_total = 0.0;
    let mut total_power = 0.0;

    for (w, &xw) in workloads.iter().zip(x.iter()) {
        vdot_total += xw * w.delta_v;
        total_power += xw * w.power;
    }

    // Avoid divide-by-zero: if power ≈ 0, intelligence is defined as 0.
    let i_scalar = if total_power > 1e-9 {
        -vdot_total / total_power
    } else {
        0.0
    };

    IntelligenceScore {
        vdot_total,
        total_power,
        i_scalar,
    }
}

/// Simple convex projection onto the probability simplex {x ≥ 0, Σ x = 1}.
///
/// This keeps allocation variables in a convex, normalized set.
fn project_to_simplex(mut x: Vec<f64>) -> Vec<f64> {
    // Algorithm: sort, find threshold, shrink – standard simplex projection.
    let n = x.len();
    if n == 0 {
        return x;
    }

    let mut u: Vec<OrderedFloat<f64>> = x.iter().map(|&xi| OrderedFloat(-xi)).collect();
    u.sort(); // descending in terms of original x

    let mut css = 0.0;
    let mut rho = -1;
    for (i, &ui) in u.iter().enumerate() {
        let xi = -ui.into_inner();
        css += xi;
        let t = (css - 1.0) / ((i + 1) as f64);
        if xi - t > 0.0 {
            rho = i as i32;
        }
    }

    let mut theta = 0.0;
    if rho >= 0 {
        let mut css2 = 0.0;
        for i in 0..=rho as usize {
            let xi = -u[i].into_inner();
            css2 += xi;
        }
        theta = (css2 - 1.0) / ((rho as usize + 1) as f64);
    }

    for xi in x.iter_mut() {
        *xi = (*xi - theta).max(0.0);
    }

    // Normalize for numerical stability.
    let sum: f64 = x.iter().sum();
    if sum > 1e-9 {
        for xi in x.iter_mut() {
            *xi /= sum;
        }
    }

    x
}

/// Compute a convex band-tightness penalty given workloads and bands.
///
/// Intuition: workloads with small residuals r_i near a corridor wall
/// push soft_start/hard_ceiling inward. The penalty is the weighted
/// sum of normalized residuals, which is convex in x.
///
/// penalty(x) = Σ_w x_w * φ(residuals_w),
/// with φ chosen as an L2 norm or linear penalty.
fn band_tightness_penalty(workloads: &[WorkloadProfile], x: &[f64]) -> f64 {
    let mut penalty = 0.0;
    for (w, &xw) in workloads.iter().zip(x.iter()) {
        if xw <= 0.0 {
            continue;
        }
        let res = &w.plane_residuals.residuals;
        if res.is_empty() {
            continue;
        }
        // Simple L2 penalty over residuals.
        let mut l2 = 0.0;
        for &ri in res {
            l2 += ri * ri;
        }
        penalty += xw * l2.sqrt();
    }
    penalty
}

/// Gradient step for maximizing intelligence subject to band penalty.
///
/// We optimize a convex surrogate:
/// maximize J(x) = I(x) - λ * band_penalty(x)
/// subject to x ∈ simplex.
///
/// This routine performs a small number of projected gradient steps.
fn solve_allocation_convex(
    workloads: &[WorkloadProfile],
    lambda_band: f64,
    max_iters: usize,
    step_size: f64,
) -> Vec<f64> {
    let n = workloads.len();
    if n == 0 {
        return Vec::new();
    }

    // Start from uniform allocation.
    let mut x = vec![1.0 / n as f64; n];

    for _ in 0..max_iters {
        // Numerical gradient via symmetric difference – small but sufficient.
        let base_intel = intelligence_for_allocation(workloads, &x);
        let base_penalty = band_tightness_penalty(workloads, &x);
        let base_obj = base_intel.i_scalar - lambda_band * base_penalty;

        let mut grad = vec![0.0; n];
        let eps = 1e-4;

        for i in 0..n {
            let mut x_plus = x.clone();
            let mut x_minus = x.clone();

            x_plus[i] = (x_plus[i] + eps).max(0.0);
            x_minus[i] = (x_minus[i] - eps).max(0.0);

            x_plus = project_to_simplex(x_plus);
            x_minus = project_to_simplex(x_minus);

            let intel_plus = intelligence_for_allocation(workloads, &x_plus);
            let pen_plus = band_tightness_penalty(workloads, &x_plus);
            let obj_plus = intel_plus.i_scalar - lambda_band * pen_plus;

            let intel_minus = intelligence_for_allocation(workloads, &x_minus);
            let pen_minus = band_tightness_penalty(workloads, &x_minus);
            let obj_minus = intel_minus.i_scalar - lambda_band * pen_minus;

            grad[i] = (obj_plus - obj_minus) / (2.0 * eps);
        }

        // Gradient ascent step.
        for i in 0..n {
            x[i] += step_size * grad[i];
        }

        // Project back to simplex to maintain convex constraints.
        x = project_to_simplex(x);
    }

    x
}

/// Monotone band-update rule.
///
/// Given current bands and workload residuals, tighten bands
/// (move soft_start upward, move hard_ceiling downward) in a way
/// that is monotone non-loosening:
///   soft_start_new ≥ soft_start_old,
///   hard_ceiling_new ≤ hard_ceiling_old.
///
/// The tightening magnitude is proportional to a scalar summary
/// of allocation-weighted residuals.
fn propose_band_updates(
    problem: &BandTighteningProblem,
    x: &[f64],
    tightening_gain: f64,
) -> Vec<CorridorBandUpdate> {
    let mut updates = Vec::new();

    // Compute a global residual summary per corridor – here we use a simple
    // average over workloads weighted by x_w; in practice you might map
    // specific residual indices to specific corridors.
    let n = problem.workloads.len();

    for (corridor_id, bands) in problem.bands.iter() {
        // Aggregate residuals across workloads.
        let mut agg_residual = 0.0;
        let mut weight_sum = 0.0;

        for (w, &xw) in problem.workloads.iter().zip(x.iter()) {
            if xw <= 0.0 {
                continue;
            }
            if w.plane_residuals.residuals.is_empty() {
                continue;
            }
            // Use mean residual for this workload as a proxy.
            let mut sum_r = 0.0;
            for &ri in w.plane_residuals.residuals.iter() {
                sum_r += ri.max(0.0);
            }
            let mean_r = sum_r / (w.plane_residuals.residuals.len() as f64);
            agg_residual += xw * mean_r;
            weight_sum += xw;
        }

        let agg = if weight_sum > 1e-9 {
            agg_residual / weight_sum
        } else {
            0.0
        };

        // Tightening magnitude – scaled by residual and gain, bounded.
        let tighten_soft = (tightening_gain * agg).min(0.05); // cap to avoid jumps
        let tighten_hard = (tightening_gain * agg).min(0.05);

        let mut soft_new = bands.soft_start + tighten_soft;
        let mut hard_new = bands.hard_ceiling - tighten_hard;

        // Enforce ordering and bounds.
        if soft_new > hard_new {
            // Collapse to a minimal, still-ordered band.
            soft_new = hard_new.max(soft_new - 1e-3);
        }
        soft_new = soft_new.clamp(0.0, 1.0);
        hard_new = hard_new.clamp(0.0, 1.0);

        // Monotone non-loosening: ensure we never loosen.
        if soft_new < bands.soft_start {
            soft_new = bands.soft_start;
        }
        if hard_new > bands.hard_ceiling {
            hard_new = bands.hard_ceiling;
        }

        updates.push(CorridorBandUpdate {
            corridor_id: corridor_id.clone(),
            soft_start_new: soft_new,
            hard_ceiling_new: hard_new,
        });
    }

    updates
}

/// Main entrypoint: solve the convex allocation and produce band updates.
pub fn schedule_workloads(problem: BandTighteningProblem) -> Result<SchedulerResult> {
    let workloads = problem.workloads.clone();
    if workloads.is_empty() {
        return Ok(SchedulerResult {
            allocation: Vec::new(),
            intelligence: IntelligenceScore {
                vdot_total: 0.0,
                total_power: 0.0,
                i_scalar: 0.0,
            },
            band_updates: Vec::new(),
        });
    }

    // Solve convex allocation on the simplex.
    let x = solve_allocation_convex(&workloads, problem.lambda_band, 32, 0.1);

    // Compute intelligence for this allocation.
    let intel = intelligence_for_allocation(&workloads, &x);

    // Compute monotone band updates.
    let band_updates = propose_band_updates(&problem, &x, 0.5);

    // Return allocation as (workload_id, x_w) pairs.
    let allocation = workloads
        .iter()
        .zip(x.iter())
        .map(|(w, &xw)| (w.id.clone(), xw))
        .collect();

    Ok(SchedulerResult {
        allocation,
        intelligence: intel,
        band_updates,
    })
}

/// Observability integration trait – to be implemented by nanorobotics-observability-core.
///
/// This stays abstract here; the concrete implementations live in
/// `crates/nanorobotics-observability-core`.
pub trait IntelligenceObservability {
    fn observe_intelligence(&self, score: &IntelligenceScore);
    fn observe_allocation(&self, allocation: &[(String, f64)]);
    fn observe_band_updates(&self, updates: &[CorridorBandUpdate]);
}

/// Corridor proposal integration trait – to be implemented by ker-corridor-proposal.
///
/// This trait attaches the band updates to ALN-backed corridor envelopes
/// and ensures monotone tightening is proven via Kani in the proposal crate.
pub trait CorridorProposalSink {
    fn submit_band_updates(&self, updates: &[CorridorBandUpdate]) -> Result<()>;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_simple_scheduler_monotone_bands() {
        let workloads = vec![
            WorkloadProfile {
                id: "w1".to_string(),
                delta_v: -0.5,
                power: 10.0,
                lane: Lane::Production,
                ker_tier: KerTier::Tier2,
                plane_residuals: PlaneResiduals {
                    residuals: vec![0.1, 0.2],
                },
            },
            WorkloadProfile {
                id: "w2".to_string(),
                delta_v: -0.2,
                power: 5.0,
                lane: Lane::Production,
                ker_tier: KerTier::Tier1,
                plane_residuals: PlaneResiduals {
                    residuals: vec![0.05, 0.1],
                },
            },
        ];

        let bands = vec![(
            "corridor-lyap".to_string(),
            CorridorBands {
                soft_start: 0.2,
                hard_ceiling: 0.8,
            },
        )];

        let problem = BandTighteningProblem {
            workloads,
            bands: bands.clone(),
            lambda_band: 0.1,
        };

        let result = schedule_workloads(problem).unwrap();
        assert_eq!(result.allocation.len(), 2);

        // Intelligence must be non-negative since delta_v are negative (reducing V).
        assert!(result.intelligence.i_scalar >= 0.0);

        // Band updates must be monotone non-loosening.
        for upd in result.band_updates.iter() {
            let orig = bands.iter().find(|(id, _)| id == &upd.corridor_id).unwrap().1;
            assert!(upd.soft_start_new >= orig.soft_start);
            assert!(upd.hard_ceiling_new <= orig.hard_ceiling);
        }
    }
}
