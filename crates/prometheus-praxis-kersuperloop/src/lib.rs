// filename: crates/prometheus-praxis-kersuperloop/src/lib.rs
// destination: https://github.com/mk-bluebird/Prometheus-Praxis/crates/prometheus-praxis-kersuperloop/src/lib.rs
// edition: 2024
// rust-version = "1.85"
// license: MIT OR Apache-2.0

#![forbid(unsafe_code)]

// Prometheus-Praxis KER-Lyapunov superloop for Phoenix.
// Non-actuating: computes K,E,R, Lyapunov residual Vt, and always-improve
// diagnostics over Phoenix workload windows, ecosafety envelopes, and
// Cyboquatic machinery spines. All outputs are recognition-only.

use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use thiserror::Error;

/// Scalar type used across KER-Lyapunov diagnostics.
pub type Scalar = f64;

/// Knowledge, eco-impact, and risk factors for this crate.
/// These are coarse, crate-level metadata aligned with your
/// existing CyboquaticCore K/E/R factors.[file:13]
pub const KFACTOR: Scalar = 0.95;
pub const EFACTOR: Scalar = 0.94;
pub const RFACTOR: Scalar = 0.09;

/// Error type for the superloop. All failures are purely diagnostic.
#[derive(Debug, Error)]
pub enum SuperloopError {
    #[error("SQLite spine error: {0}")]
    SpineError(String),
    #[error("Input corridor violation: {0}")]
    CorridorViolation(String),
    #[error("Invariant violation: {0}")]
    InvariantViolation(String),
}

/// Direction of Lyapunov residual change across a window.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum ResidualDirection {
    /// Vt decreased or stayed equal → stable or improved.
    NonPositive,
    /// Vt increased → destabilizing change.
    Positive,
}

/// Per-window Lyapunov residual summary for Phoenix.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResidualWindowSummary {
    pub node_id: String,
    pub region: String,
    pub window_start_utc: DateTime<Utc>,
    pub window_end_utc: DateTime<Utc>,
    pub mean_vt_before: Scalar,
    pub mean_vt_after: Scalar,
    pub delta_vt: Scalar,
    pub direction: ResidualDirection,
}

/// KER triad summary for a Phoenix shard or node.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerSummary {
    /// Knowledge / evidence factor in [0, 1].
    pub k: Scalar,
    /// Eco-impact (benefit) factor in [0, 1].
    pub e: Scalar,
    /// Risk-of-harm factor in [0, 1].
    pub r: Scalar,
    /// Convenience score s = k + e - r in [0, 1], clamped.
    pub score: Scalar,
}

impl KerSummary {
    /// Construct a KerSummary, enforcing [0,1] bounds and deriving score.
    pub fn new(k: Scalar, e: Scalar, r: Scalar) -> Result<Self, SuperloopError> {
        if !(0.0..=1.0).contains(&k) {
            return Err(SuperloopError::CorridorViolation(format!(
                "K outside [0,1]: {}",
                k
            )));
        }
        if !(0.0..=1.0).contains(&e) {
            return Err(SuperloopError::CorridorViolation(format!(
                "E outside [0,1]: {}",
                e
            )));
        }
        if !(0.0..=1.0).contains(&r) {
            return Err(SuperloopError::CorridorViolation(format!(
                "R outside [0,1]: {}",
                r
            )));
        }
        let mut s = k + e - r;
        if s < 0.0 {
            s = 0.0;
        } else if s > 1.0 {
            s = 1.0;
        }
        Ok(Self { k, e, r, score: s })
    }
}

/// Always-improve score on top of KER and Lyapunov residuals.[file:14]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AlwaysImproveScore {
    /// Effective energy efficiency E_eff = totalsurplusJ / totalreqJ.
    pub e_eff: Scalar,
    /// Carbon risk coordinate C_r in [0,1].
    pub c_r: Scalar,
    /// Biodiversity risk coordinate B_r in [0,1].
    pub b_r: Scalar,
    /// Lyapunov mean residual delta over window (Vt_after - Vt_before).
    pub delta_vt: Scalar,
    /// Composite always-improve score S_AI.
    pub s_ai: Scalar,
    /// Constraint flags (all MUST be true for admissible improvement).
    pub carbon_ceiling_ok: bool,
    pub residual_nonpositive_ok: bool,
}

/// Hard-coded corridor ceilings consistent with your existing spine:
/// - Carbon risk ceiling C_r <= 0.13.[file:14]
/// - Delta Vt must be <= 0.0 for non-regressive workloads.[file:14]
pub const CARBON_RISK_CEILING: Scalar = 0.13;

/// Compute AlwaysImproveScore for a single Phoenix workload window.[file:14]
///
/// Inputs:
/// - total_req_j: total requested energy in the window (J).
/// - total_surplus_j: total surplus energy available in the window (J).
/// - mean_carbon_risk: mean carbon risk coordinate C_r in [0,1].
/// - mean_biodiv_risk: mean biodiversity risk coordinate B_r in [0,1].
/// - mean_vt_before: mean Lyapunov residual before workloads.
/// - mean_vt_after: mean Lyapunov residual after workloads.
///
/// S_AI = E_eff - C_r - B_r - delta_vt, subject to
/// - C_r <= 0.13,
/// - delta_vt <= 0.0.[file:14]
pub fn compute_always_improve(
    total_req_j: Scalar,
    total_surplus_j: Scalar,
    mean_carbon_risk: Scalar,
    mean_biodiv_risk: Scalar,
    mean_vt_before: Scalar,
    mean_vt_after: Scalar,
) -> Result<AlwaysImproveScore, SuperloopError> {
    if total_req_j <= 0.0 {
        return Err(SuperloopError::CorridorViolation(
            "total_req_j must be > 0".to_string(),
        ));
    }
    if mean_carbon_risk < 0.0 || mean_carbon_risk > 1.0 {
        return Err(SuperloopError::CorridorViolation(format!(
            "carbon risk outside [0,1]: {}",
            mean_carbon_risk
        )));
    }
    if mean_biodiv_risk < 0.0 || mean_biodiv_risk > 1.0 {
        return Err(SuperloopError::CorridorViolation(format!(
            "biodiv risk outside [0,1]: {}",
            mean_biodiv_risk
        )));
    }

    let e_eff = total_surplus_j / total_req_j;
    let delta_vt = mean_vt_after - mean_vt_before;

    let carbon_ceiling_ok = mean_carbon_risk <= CARBON_RISK_CEILING;
    let residual_nonpositive_ok = delta_vt <= 0.0;

    let s_ai = e_eff - mean_carbon_risk - mean_biodiv_risk - delta_vt;

    Ok(AlwaysImproveScore {
        e_eff,
        c_r: mean_carbon_risk,
        b_r: mean_biodiv_risk,
        delta_vt,
        s_ai,
        carbon_ceiling_ok,
        residual_nonpositive_ok,
    })
}

/// Combined KER + Lyapunov residual + always-improve diagnostic
/// for a Phoenix node/time window.[file:13][file:14]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PhoenixKerWindowDiagnostic {
    pub residual: ResidualWindowSummary,
    pub ker: KerSummary,
    pub always_improve: AlwaysImproveScore,
    /// True if all hard constraints are satisfied:
    /// - C_r <= 0.13
    /// - delta Vt <= 0.0.[file:14]
    pub admissible: bool,
}

/// Minimal readonly input for the superloop; this is designed to be fed
/// from vcyboworkloadnodewindow, ecosafety envelopes, or Cyboquatic cores.[file:13][file:14]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PhoenixKerWindowInput {
    pub node_id: String,
    pub region: String,
    pub window_start_utc: DateTime<Utc>,
    pub window_end_utc: DateTime<Utc>,
    pub total_req_j: Scalar,
    pub total_surplus_j: Scalar,
    pub mean_vt_before: Scalar,
    pub mean_vt_after: Scalar,
    pub mean_carbon_risk: Scalar,
    pub mean_biodiv_risk: Scalar,
    pub k: Scalar,
    pub e: Scalar,
    pub r: Scalar,
}

/// Superloop evaluation: given a PhoenixKerWindowInput, compute
/// KER, residual, and always-improve diagnostics.[file:13][file:14]
///
/// This function is non-actuating and can be safely exposed to
/// AI-chat tooling and EcoNet agents; it never touches hardware
/// or control paths.
pub fn evaluate_phoenix_window(
    input: PhoenixKerWindowInput,
) -> Result<PhoenixKerWindowDiagnostic, SuperloopError> {
    // KER triad.
    let ker = KerSummary::new(input.k, input.e, input.r)?;

    // Residual summary.
    let delta_vt = input.mean_vt_after - input.mean_vt_before;
    let direction = if delta_vt <= 0.0 {
        ResidualDirection::NonPositive
    } else {
        ResidualDirection::Positive
    };
    let residual = ResidualWindowSummary {
        node_id: input.node_id.clone(),
        region: input.region.clone(),
        window_start_utc: input.window_start_utc,
        window_end_utc: input.window_end_utc,
        mean_vt_before: input.mean_vt_before,
        mean_vt_after: input.mean_vt_after,
        delta_vt,
        direction,
    };

    // Always-improve score.
    let ai = compute_always_improve(
        input.total_req_j,
        input.total_surplus_j,
        input.mean_carbon_risk,
        input.mean_biodiv_risk,
        input.mean_vt_before,
        input.mean_vt_after,
    )?;

    let admissible = ai.carbon_ceiling_ok && ai.residual_nonpositive_ok;

    Ok(PhoenixKerWindowDiagnostic {
        residual,
        ker,
        always_improve: ai,
        admissible,
    })
}

// --- OPTIONAL: Kani harness stub (non-optional Kani, precise version lives in a dedicated crate) ---
// This file assumes that a separate `prometheus-praxis-kersuperloop-kani` crate
// brings in `kani-verifier = "0.67"` and re-uses these types for proofs.
// No Kani dependency is introduced here to keep this crate non-actuating and
// free of verification tooling in its direct dependency graph.[file:13]
