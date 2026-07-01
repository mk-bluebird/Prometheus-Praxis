// path: crates/prometheus_praxis_ai/src/always_improve.rs
// role: Non-actuating scoring kernel for "always-improve" ranking of
//       diagnostic shards and nodes, plus Kani harness stubs.
// edition: 2024, rust-version = "1.85"

#![forbid(unsafe_code)]

use rust_decimal::Decimal;
use rust_decimal::prelude::ToPrimitive;

// Pull in KER and Lyapunov snapshots from existing governance crates.
use prometheus_praxisker::KerOutput;
use prometheus_praxis::governance::{
    ActionLane,
    LyapunovResidualSnapshot,
    RohSnapshot,
};

/// Bounded scalar in [0,1] for ranking.
/// Interpreted as "how strong is the evidence that this object/window
/// is strictly improving or at least not degrading under KER + Lyapunov".
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct AlwaysImproveScore {
    pub score: Decimal,      // 0.0 .. 1.0
    pub safe_to_promote: bool,
}

/// Configuration parameters for scoring.
/// These should be wired from Cargo.toml workspace.metadata.ker.residuals
/// and/or an ALN shard; here we keep them explicit for clarity.
#[derive(Debug, Clone, Copy)]
pub struct AlwaysImproveConfig {
    /// Reference Lyapunov value, typically 1.0.
    pub v_ref: Decimal,
    /// Max allowed Lyapunov increase (noise band).
    pub max_delta_v: Decimal,
    /// Weight for K residual.
    pub w_k: Decimal,
    /// Weight for E residual.
    pub w_e: Decimal,
    /// Weight for R residual.
    pub w_r: Decimal,
}

impl Default for AlwaysImproveConfig {
    fn default() -> Self {
        // These numbers should be synced with workspace.metadata.ker.residuals
        // and KER/observability shards; adjust in one place and propagate.
        Self {
            v_ref: Decimal::ONE,
            max_delta_v: Decimal::new(2, 2), // 0.02
            w_k: Decimal::new(5, 1),         // 0.5
            w_e: Decimal::new(3, 1),         // 0.3
            w_r: Decimal::new(2, 1),         // 0.2
        }
    }
}

/// Helper: clamp a Decimal into [0,1].
fn clamp_01(x: Decimal) -> Decimal {
    if x < Decimal::ZERO {
        Decimal::ZERO
    } else if x > Decimal::ONE {
        Decimal::ONE
    } else {
        x
    }
}

/// Compute a normalized residual for K,E,R given lane thresholds.
/// - For K,E: residual is (target - actual), saturated at 0 when actual >= target.
/// - For R:   residual is (actual - target), saturated at 0 when actual <= target.
///
/// All values are assumed already in [0,1].
fn ker_residuals_for_lane(
    lane: ActionLane,
    ker: &KerOutput,
    k_min_research: Decimal,
    k_min_pilot: Decimal,
    k_min_prod: Decimal,
    e_min_research: Decimal,
    e_min_pilot: Decimal,
    e_min_prod: Decimal,
    r_max_research: Decimal,
    r_max_pilot: Decimal,
    r_max_prod: Decimal,
) -> (Decimal, Decimal, Decimal) {
    let (k_target, e_target, r_target) = match lane {
        ActionLane::Research => (k_min_research, e_min_research, r_max_research),
        ActionLane::Pilot => (k_min_pilot, e_min_pilot, r_max_pilot),
        ActionLane::Production => (k_min_prod, e_min_prod, r_max_prod),
    };

    let k = clamp_01(ker.k);
    let e = clamp_01(ker.e);
    let r = clamp_01(ker.r);

    let r_k = (k_target - k).max(Decimal::ZERO);
    let r_e = (e_target - e).max(Decimal::ZERO);
    let r_r = (r - r_target).max(Decimal::ZERO);

    (r_k, r_e, r_r)
}

/// Compute Lyapunov residual: V_next - V_current.
/// Assumes values are already bounded in [0,1].
fn lyapunov_delta(res: &LyapunovResidualSnapshot) -> Decimal {
    let v_current = clamp_01(res.v_current);
    let v_next = clamp_01(res.v_next);
    v_next - v_current
}

/// Main scoring function.
/// Inputs:
/// - lane: lane of the object/window (RESEARCH/PILOT/PRODUCTION).
/// - ker: K,E,R scores for this object/window.
/// - roh: RoH snapshot (scalar in [0,1]).
/// - lyap: Lyapunov residual snapshot for this object/window.
/// - cfg: scoring config (weights, v_ref, max_delta_v).
///
/// Output:
/// - AlwaysImproveScore with score in [0,1] and safe_to_promote flag.
///
/// Semantics:
/// - If RoH > global ceiling or Lyapunov delta > max_delta_v: score = 0, safe_to_promote = false.
/// - Otherwise, combine normalized K,E,R residuals into a single residual and invert to score.
///   score = clamp_01(1 - (w_k * r_k + w_e * r_e + w_r * r_r)).
pub fn compute_always_improve_score(
    lane: ActionLane,
    ker: &KerOutput,
    roh: &RohSnapshot,
    lyap: &LyapunovResidualSnapshot,
    cfg: AlwaysImproveConfig,
    roh_ceiling_global: Decimal,
    k_min_research: Decimal,
    k_min_pilot: Decimal,
    k_min_prod: Decimal,
    e_min_research: Decimal,
    e_min_pilot: Decimal,
    e_min_prod: Decimal,
    r_max_research: Decimal,
    r_max_pilot: Decimal,
    r_max_prod: Decimal,
) -> AlwaysImproveScore {
    let roh_val = clamp_01(roh.roh);
    if roh_val > roh_ceiling_global {
        return AlwaysImproveScore {
            score: Decimal::ZERO,
            safe_to_promote: false,
        };
    }

    let delta_v = lyapunov_delta(lyap);
    if delta_v > cfg.max_delta_v {
        return AlwaysImproveScore {
            score: Decimal::ZERO,
            safe_to_promote: false,
        };
    }

    let (r_k, r_e, r_r) = ker_residuals_for_lane(
        lane,
        ker,
        k_min_research,
        k_min_pilot,
        k_min_prod,
        e_min_research,
        e_min_pilot,
        e_min_prod,
        r_max_research,
        r_max_pilot,
        r_max_prod,
    );

    let combined = cfg.w_k * r_k + cfg.w_e * r_e + cfg.w_r * r_r;
    let score = clamp_01(Decimal::ONE - combined);

    AlwaysImproveScore {
        score,
        safe_to_promote: score > Decimal::new(7, 1), // > 0.7
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn d(x: f32) -> Decimal {
        Decimal::from_f32(x).unwrap()
    }

    #[test]
    fn basic_monotone_behavior() {
        let cfg = AlwaysImproveConfig::default();
        let roh_ceiling = d(0.30);

        let ker = KerOutput {
            k: d(0.95),
            e: d(0.90),
            r: d(0.10),
        };

        let roh_ok = RohSnapshot { roh: d(0.20) };
        let lyap_ok = LyapunovResidualSnapshot {
            v_current: d(0.50),
            v_next: d(0.49),
            epsilon: d(0.02),
        };

        let score = compute_always_improve_score(
            ActionLane::Production,
            &ker,
            &roh_ok,
            &lyap_ok,
            cfg,
            roh_ceiling,
            d(0.70),
            d(0.80),
            d(0.92),
            d(0.60),
            d(0.75),
            d(0.88),
            d(0.25),
            d(0.20),
            d(0.15),
        );

        assert!(score.safe_to_promote);
        assert!(score.score > d(0.7));
    }
}

// Kani harness stubs — place in a dedicated tests crate or enable with cfg feature.
// They assert that if K/E meet lane minima, R below max, RoH <= ceiling,
// and Lyapunov delta <= max_delta_v, then safe_to_promote is never false.

#[cfg(kani)]
mod kani_harnesses {
    use super::*;
    use kani::any;

    fn bounded_01(x: f32) -> Decimal {
        let mut d = Decimal::from_f32(x).unwrap_or(Decimal::ZERO);
        if d < Decimal::ZERO {
            d = Decimal::ZERO;
        } else if d > Decimal::ONE {
            d = Decimal::ONE;
        }
        d
    }

    /// Kani proof stub for RESEARCH lane monotonicity of safe_to_promote.
    #[kani::proof]
    fn always_improve_does_not_block_when_invariants_hold_research() {
        let cfg = AlwaysImproveConfig::default();
        let roh_ceiling = bounded_01(any());

        let ker = KerOutput {
            k: bounded_01(any()),
            e: bounded_01(any()),
            r: bounded_01(any()),
        };

        let roh = RohSnapshot {
            roh: bounded_01(any()),
        };

        let lyap = LyapunovResidualSnapshot {
            v_current: bounded_01(any()),
            v_next: bounded_01(any()),
            epsilon: bounded_01(any()),
        };

        // Lane thresholds (symbolic but ordered).
        let k_min_research = bounded_01(any());
        let e_min_research = bounded_01(any());
        let r_max_research = bounded_01(any());

        // Assume K,E >= minima and R <= max.
        kani::assume(ker.k >= k_min_research);
        kani::assume(ker.e >= e_min_research);
        kani::assume(ker.r <= r_max_research);

        // Assume RoH within ceiling.
        kani::assume(roh.roh <= roh_ceiling);

        // Assume Lyapunov delta <= cfg.max_delta_v.
        let delta_v = lyapunov_delta(&lyap);
        kani::assume(delta_v <= cfg.max_delta_v);

        // Use permissive thresholds for other lanes (not used here).
        let k_min_pilot = Decimal::ZERO;
        let k_min_prod = Decimal::ZERO;
        let e_min_pilot = Decimal::ZERO;
        let e_min_prod = Decimal::ZERO;
        let r_max_pilot = Decimal::ONE;
        let r_max_prod = Decimal::ONE;

        let out = compute_always_improve_score(
            ActionLane::Research,
            &ker,
            &roh,
            &lyap,
            cfg,
            roh_ceiling,
            k_min_research,
            k_min_pilot,
            k_min_prod,
            e_min_research,
            e_min_pilot,
            e_min_prod,
            r_max_research,
            r_max_pilot,
            r_max_prod,
        );

        // Under these assumptions, safe_to_promote must not be false.
        assert!(out.safe_to_promote);
    }
}
