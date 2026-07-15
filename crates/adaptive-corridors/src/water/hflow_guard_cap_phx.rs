// CAP Phoenix Hflow guard (diagnostic-only, non-actuating).

#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Hflow guard for CAP urban flood reaches near Phoenix.
//!
//! This module provides a non-actuating diagnostic kernel that computes
//! R_vel, R_mob, R_geo, Hflow, and Hflow_op for CAP corridors and
//! returns a lane decision (Allow / Derate / Stop) plus reasons.
//!
//! It is designed to be queried by MCP agents and service tiers;
//! it never drives gates, pumps, or shutters directly.

use std::fmt;

/// Lane decision for Hflow.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum HflowLane {
    /// Corridor is safely within band; operations allowed.
    Allow,
    /// Corridor is near soft band or tail risk; operations should be derated.
    Derate,
    /// Corridor is at or above hard ceiling; operations must stop.
    Stop,
}

impl fmt::Display for HflowLane {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            HflowLane::Allow => "Allow",
            HflowLane::Derate => "Derate",
            HflowLane::Stop => "Stop",
        };
        f.write_str(s)
    }
}

/// Reason codes for guard decisions, suitable for QPU and Veritas anchors.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum HflowReason {
    // Deterministic scalar above hard ceiling.
    RoHAboveHardCeiling,
    // Operational scalar (mean + beta*std) above hard ceiling.
    RoHOpAboveHardCeiling,
    // Tail probability of exceeding hard ceiling above policy threshold.
    TailProbAboveThreshold,
    // Scalar within soft band, needs derate.
    WithinSoftBand,
    // Operational scalar within soft band, needs derate.
    OpWithinSoftBand,
    // Scalar and op metric both well below soft band.
    WithinSafeBand,
    // Probabilistic guard suggests caution despite safe scalar.
    ProbabilisticCaution,
}

/// CAP corridor configuration (policy constants).
#[derive(Debug, Clone)]
pub struct CapHflowPolicy {
    /// Soft band start (e.g. 0.24).
    pub soft_band_start: f32,
    /// Hard ceiling (e.g. 0.30).
    pub hard_ceiling: f32,
    /// Beta multiplier for variance in Hflow_op.
    pub beta: f32,
    /// Maximum allowed exceedance probability for Allow lane.
    pub max_exceed_prob: f32,
    /// Weights for R_vel, R_mob, R_geo -> Hflow.
    pub w_vel: f32,
    pub w_mob: f32,
    pub w_geo: f32,
}

/// CAP corridor telemetry snapshot (10–15 min window).
///
/// All raw values are in physical units; normalization is done
/// inside the guard using corridor envelopes.
#[derive(Debug, Clone)]
pub struct CapHflowTelemetry {
    // Hydraulics
    pub velocity_mps: f32,
    pub turbulence_index: f32,
    pub shear_index: f32,
    // Shutter / gate state
    pub shutter_open_frac: f32,
    pub shutter_rate_per_min: f32,
    // Mobility and geometrics
    pub mobility_index: f32,
    pub geom_stability_index: f32,
}

/// Probabilistic summary for Hflow, coming from density estimation.
///
/// This mirrors your sediment RoH pattern: E[H], Var(H), and tail probability.
#[derive(Debug, Clone)]
pub struct HflowProbSummary {
    /// Estimated mean of Hflow.
    pub mean: f32,
    /// Estimated variance of Hflow.
    pub var: f32,
    /// Estimated probability that Hflow exceeds hard ceiling.
    pub tail_prob_over_ceiling: f32,
}

/// Full guard result for one corridor window.
#[derive(Debug, Clone)]
pub struct HflowGuardResult {
    /// Corridor identifier (e.g. PHX_CAP_URBAN_FLOOD_V1).
    pub corridor_id: String,
    /// Deterministic scalar Hflow.
    pub hflow_scalar: f32,
    /// Operational scalar Hflow_op = E[H] + beta * sqrt(Var(H)).
    pub hflow_op: f32,
    /// Sub-risk coordinates.
    pub r_vel: f32,
    pub r_mob: f32,
    pub r_geo: f32,
    /// Lane decision.
    pub lane: HflowLane,
    /// Reason codes for governance and diagnostics.
    pub reasons: Vec<HflowReason>,
    /// Tail probability of exceeding hard ceiling.
    pub tail_prob_over_ceiling: f32,
}

// Corridor-normalization helpers for CAP Phoenix.
// Design envelopes (see previous turn) are encoded as constants.
/// Normalize velocity 0.5–2.5 m/s to 0–1.
fn norm_velocity(v: f32) -> f32 {
    let num = v - 0.5;
    let den = 2.5 - 0.5;
    ((num / den).clamp(0.0, 1.0))
}

/// Normalize turbulence 0.0–0.6 to 0–1.
fn norm_turbulence(t: f32) -> f32 {
    (t / 0.6).clamp(0.0, 1.0)
}

/// Normalize shear 0.0–1.0 to 0–1.
fn norm_shear(s: f32) -> f32 {
    s.clamp(0.0, 1.0)
}

/// Normalize shutter opening 0.2–1.0 to 0–1.
fn norm_shutter_open(frac: f32) -> f32 {
    let num = frac - 0.2;
    let den = 1.0 - 0.2;
    ((num / den).clamp(0.0, 1.0))
}

/// Normalize shutter rate 0.0–0.1 per minute to 0–1.
fn norm_shutter_rate(rate: f32) -> f32 {
    (rate / 0.1).clamp(0.0, 1.0)
}

/// Normalize mobility 0–1 (already normalized).
fn norm_mobility(mu: f32) -> f32 {
    mu.clamp(0.0, 1.0)
}

/// Normalize geom stability 0–1 (already normalized).
fn norm_geom_stability(g: f32) -> f32 {
    g.clamp(0.0, 1.0)
}

/// Compute sub-risk coordinates R_vel, R_mob, R_geo given telemetry.
///
/// Uses corridor-specific weights documented in the Phoenix CAP design.
fn compute_sub_risks(t: &CapHflowTelemetry) -> (f32, f32, f32) {
    // Normalize
    let v_hat = norm_velocity(t.velocity_mps);
    let turb_hat = norm_turbulence(t.turbulence_index);
    let shear_hat = norm_shear(t.shear_index);

    let s_open_hat = norm_shutter_open(t.shutter_open_frac);
    let s_rate_hat = norm_shutter_rate(t.shutter_rate_per_min);
    let mu_hat = norm_mobility(t.mobility_index);

    let g_hat = norm_geom_stability(t.geom_stability_index);

    // Corridor-local weights
    let r_vel = 0.4 * v_hat + 0.3 * turb_hat + 0.3 * shear_hat;
    let r_mob = 0.3 * s_open_hat + 0.3 * s_rate_hat + 0.4 * mu_hat;
    let r_geo = g_hat;

    (r_vel, r_mob, r_geo)
}

/// Compute deterministic Hflow from sub-risks and policy weights.
fn compute_hflow_scalar(policy: &CapHflowPolicy, r_vel: f32, r_mob: f32, r_geo: f32) -> f32 {
    policy.w_vel * r_vel + policy.w_mob * r_mob + policy.w_geo * r_geo
}

/// Compute Hflow_op = mean + beta * sqrt(var).
fn compute_hflow_op(policy: &CapHflowPolicy, prob: &HflowProbSummary) -> f32 {
    let std = prob.var.max(0.0).sqrt();
    prob.mean + policy.beta * std
}

/// Decide lane using deterministic and probabilistic metrics.
///
/// This mirrors your sediment RoH guard pattern:
/// - Any scalar or op metric >= hard ceiling -> Stop.
/// - Tail probability above max_exceed_prob -> Stop or Derate.
/// - Soft band start used to choose Allow vs Derate.
fn decide_lane(
    policy: &CapHflowPolicy,
    h_scalar: f32,
    h_op: f32,
    tail_prob: f32,
    reasons_out: &mut Vec<HflowReason>,
) -> HflowLane {
    // Hard ceiling checks first: Stop if any exceedance.
    if h_scalar >= policy.hard_ceiling {
        reasons_out.push(HflowReason::RoHAboveHardCeiling);
        return HflowLane::Stop;
    }
    if h_op >= policy.hard_ceiling {
        reasons_out.push(HflowReason::RoHOpAboveHardCeiling);
        return HflowLane::Stop;
    }
    if tail_prob > policy.max_exceed_prob {
        // Tail probability above threshold: at least Derate.
        reasons_out.push(HflowReason::TailProbAboveThreshold);
        // If scalar is close to hard ceiling, escalate to Stop.
        if h_scalar >= policy.soft_band_start {
            return HflowLane::Stop;
        }
        return HflowLane::Derate;
    }

    // At this point, tail prob is <= max_exceed_prob and both scalars < hard ceiling.

    // Soft band handling: Derate when within soft band.
    if h_scalar >= policy.soft_band_start {
        reasons_out.push(HflowReason::WithinSoftBand);
        return HflowLane::Derate;
    }
    if h_op >= policy.soft_band_start {
        reasons_out.push(HflowReason::OpWithinSoftBand);
        return HflowLane::Derate;
    }

    // Safe band: Allow, but mark that it is within safe band.
    reasons_out.push(HflowReason::WithinSafeBand);
    HflowLane::Allow
}

/// Run guard for one corridor window.
///
/// This is the main entry point that MCP tools and service tiers call.
/// It performs no actuation; it only returns diagnostics.
pub fn evaluate_cap_hflow(
    corridor_id: impl Into<String>,
    policy: &CapHflowPolicy,
    telemetry: &CapHflowTelemetry,
    prob_summary: &HflowProbSummary,
) -> HflowGuardResult {
    let (r_vel, r_mob, r_geo) = compute_sub_risks(telemetry);
    let h_scalar = compute_hflow_scalar(policy, r_vel, r_mob, r_geo);
    let h_op = compute_hflow_op(policy, prob_summary);

    let mut reasons = Vec::new();
    let lane = decide_lane(
        policy,
        h_scalar,
        h_op,
        prob_summary.tail_prob_over_ceiling,
        &mut reasons,
    );

    HflowGuardResult {
        corridor_id: corridor_id.into(),
        hflow_scalar: h_scalar,
        hflow_op: h_op,
        r_vel,
        r_mob,
        r_geo,
        lane,
        reasons,
        tail_prob_over_ceiling: prob_summary.tail_prob_over_ceiling,
    }
}

/// Default CAP Phoenix policy for URBAN_FLOOD reach.
///
/// This matches the design bands and weights described in the corridor spec.
pub fn default_cap_phx_policy() -> CapHflowPolicy {
    CapHflowPolicy {
        soft_band_start: 0.24,
        hard_ceiling: 0.30,
        beta: 2.0,
        max_exceed_prob: 0.01,
        w_vel: 0.5,
        w_mob: 0.3,
        w_geo: 0.2,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn stop_when_scalar_above_ceiling() {
        let policy = default_cap_phx_policy();
        let telemetry = CapHflowTelemetry {
            velocity_mps: 1.8,
            turbulence_index: 0.36,
            shear_index: 0.7,
            shutter_open_frac: 0.5,
            shutter_rate_per_min: 0.06,
            mobility_index: 0.5,
            geom_stability_index: 0.3,
        };
        let prob = HflowProbSummary {
            mean: 0.44,
            var: 0.01,
            tail_prob_over_ceiling: 0.12,
        };

        let result = evaluate_cap_hflow("PHX_CAP_URBAN_FLOOD_V1", &policy, &telemetry, &prob);
        assert_eq!(result.lane, HflowLane::Stop);
        assert!(result.hflow_scalar >= policy.hard_ceiling);
    }

    #[test]
    fn allow_in_safe_band_low_tail_prob() {
        let policy = default_cap_phx_policy();
        let telemetry = CapHflowTelemetry {
            velocity_mps: 0.9,
            turbulence_index: 0.12,
            shear_index: 0.3,
            shutter_open_frac: 0.8,
            shutter_rate_per_min: 0.02,
            mobility_index: 0.2,
            geom_stability_index: 0.2,
        };
        let prob = HflowProbSummary {
            mean: 0.15,
            var: 0.0025,
            tail_prob_over_ceiling: 0.001,
        };

        let result = evaluate_cap_hflow("PHX_CAP_URBAN_FLOOD_V1", &policy, &telemetry, &prob);
        assert_eq!(result.lane, HflowLane::Allow);
        assert!(result.hflow_scalar < policy.soft_band_start);
        assert!(result.hflow_op < policy.soft_band_start);
        assert!(result.tail_prob_over_ceiling <= policy.max_exceed_prob);
    }
}
