// filename: crates/ecosafety-core-v2/src/lib.rs
// destination: https://github.com/mk-bluebird/Prometheus-Praxis/crates/ecosafety-core-v2/src/lib.rs
// edition: 2024
// rust-version = "1.85"
// license: MIT OR Apache-2.0

#![forbid(unsafe_code)]

// Ecosafety core v2: KER-Lyapunov constitution nucleus.
// Non-actuating: provides typed RiskVector, LyapunovWeights, and invariant checks.
// All physical engines (hydraulic PDEs, UHI models, AI workload planes) are clients.

use serde::{Deserialize, Serialize};
use thiserror::Error;

/// Scalar type for all risk and Lyapunov math.
pub type Scalar = f64;

/// Error type for ecosafety-core-v2.
#[derive(Debug, Error)]
pub enum EcosafetyError {
    #[error("risk coordinate outside [0,1]: {0}")]
    RiskOutOfBounds(String),
    #[error("weight must be non-negative: {0}")]
    NegativeWeight(String),
    #[error("invalid stability step: {0}")]
    StabilityStep(String),
}

/// Risk coordinates for a Phoenix urban state snapshot.
/// This is intentionally small and extensible; you can add more planes later
/// (e.g., neurorights, microplastics) without changing the invariant grammar.[file:14]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RiskVector {
    /// Normalized hydraulic surcharge risk r_h in [0,1].
    pub r_hyd: Scalar,
    /// Normalized thermal risk (e.g., UHI composite scalar) r_T in [0,1].
    pub r_thermal: Scalar,
    /// Normalized energy / infrastructure risk r_E in [0,1].
    pub r_energy: Scalar,
    /// Normalized biodiversity risk r_B in [0,1].
    pub r_biodiv: Scalar,
    /// Normalized AI workload / compute risk r_AI in [0,1].
    pub r_ai: Scalar,
}

impl RiskVector {
    /// Construct a RiskVector, enforcing corridor bounds [0,1] on each coordinate.
    pub fn new(
        r_hyd: Scalar,
        r_thermal: Scalar,
        r_energy: Scalar,
        r_biodiv: Scalar,
        r_ai: Scalar,
    ) -> Result<Self, EcosafetyError> {
        fn check01(label: &str, value: Scalar) -> Result<(), EcosafetyError> {
            if !(0.0..=1.0).contains(&value) {
                return Err(EcosafetyError::RiskOutOfBounds(format!(
                    "{label}={value}"
                )));
            }
            Ok(())
        }

        check01("r_hyd", r_hyd)?;
        check01("r_thermal", r_thermal)?;
        check01("r_energy", r_energy)?;
        check01("r_biodiv", r_biodiv)?;
        check01("r_ai", r_ai)?;

        Ok(Self {
            r_hyd,
            r_thermal,
            r_energy,
            r_biodiv,
            r_ai,
        })
    }
}

/// Lyapunov weight configuration for the multi-plane corridor.
/// All weights must be non-negative; their relative magnitudes encode
/// your hydraulic/thermal/energy/biodiversity governance priorities.[file:14]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LyapunovWeights {
    pub w_hyd: Scalar,
    pub w_thermal: Scalar,
    pub w_energy: Scalar,
    pub w_biodiv: Scalar,
    pub w_ai: Scalar,
}

impl LyapunovWeights {
    pub fn new(
        w_hyd: Scalar,
        w_thermal: Scalar,
        w_energy: Scalar,
        w_biodiv: Scalar,
        w_ai: Scalar,
    ) -> Result<Self, EcosafetyError> {
        fn check_nonneg(label: &str, value: Scalar) -> Result<(), EcosafetyError> {
            if value < 0.0 {
                return Err(EcosafetyError::NegativeWeight(format!(
                    "{label}={value}"
                )));
            }
            Ok(())
        }

        check_nonneg("w_hyd", w_hyd)?;
        check_nonneg("w_thermal", w_thermal)?;
        check_nonneg("w_energy", w_energy)?;
        check_nonneg("w_biodiv", w_biodiv)?;
        check_nonneg("w_ai", w_ai)?;

        Ok(Self {
            w_hyd,
            w_thermal,
            w_energy,
            w_biodiv,
            w_ai,
        })
    }

    /// Compute Lyapunov residual V(t) = sum_j w_j r_j^2 for the given risk vector.[file:14]
    pub fn lyapunov(&self, r: &RiskVector) -> Scalar {
        self.w_hyd * r.r_hyd * r.r_hyd
            + self.w_thermal * r.r_thermal * r.r_thermal
            + self.w_energy * r.r_energy * r.r_energy
            + self.w_biodiv * r.r_biodiv * r.r_biodiv
            + self.w_ai * r.r_ai * r.r_ai
    }
}

/// Discrete-time Lyapunov step parameters.
/// s_t encodes the "tailwind" or self-correction engine: s_t = k_t * e_t - r_t.[file:14]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LyapunovStep {
    /// Proportionality constant k_t (knowledge/experience factor).
    pub k_t: Scalar,
    /// Available stabilizing energy e_t (dimensionless or normalized Joules).
    pub e_t: Scalar,
    /// Internal resistance / "tailwind" r_t.
    pub r_t: Scalar,
}

impl LyapunovStep {
    pub fn new(k_t: Scalar, e_t: Scalar, r_t: Scalar) -> Self {
        Self { k_t, e_t, r_t }
    }

    /// Compute s_t = k_t * e_t - r_t.
    pub fn s_t(&self) -> Scalar {
        self.k_t * self.e_t - self.r_t
    }
}

/// Result of checking the discrete Lyapunov invariant
/// V(t+1) - V(t) <= -s_t.[file:14]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LyapunovCheckResult {
    pub v_t: Scalar,
    pub v_t1: Scalar,
    pub s_t: Scalar,
    /// True if V(t+1) - V(t) <= -s_t holds.
    pub invariant_ok: bool,
    /// Actual delta: V(t+1) - V(t).
    pub delta_v: Scalar,
}

/// Check the discrete Lyapunov invariant for a given step.
/// Inputs:
/// - weights: LyapunovWeights.
/// - r_t: RiskVector at time t.
/// - r_t1: RiskVector at time t+1.
/// - step: LyapunovStep (k_t, e_t, r_t).
///
/// Output:
/// - LyapunovCheckResult summarizing V(t), V(t+1), s_t, and invariant status.
pub fn check_lyapunov_invariant(
    weights: &LyapunovWeights,
    r_t: &RiskVector,
    r_t1: &RiskVector,
    step: &LyapunovStep,
) -> LyapunovCheckResult {
    let v_t = weights.lyapunov(r_t);
    let v_t1 = weights.lyapunov(r_t1);
    let s_t = step.s_t();
    let delta_v = v_t1 - v_t;
    let invariant_ok = delta_v <= -s_t;

    LyapunovCheckResult {
        v_t,
        v_t1,
        s_t,
        invariant_ok,
        delta_v,
    }
}

/// Non-offsettable plane configuration: planes whose corridors are hard limits.
/// Example: biodiversity, neurorights, carbon budget.[file:14]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NonOffsettablePlanes {
    pub max_biodiv: Scalar,
    pub max_carbon: Scalar,
    pub max_neuro: Scalar,
}

impl NonOffsettablePlanes {
    pub fn check(&self, r_biodiv: Scalar, r_carbon: Scalar, r_neuro: Scalar) -> bool {
        r_biodiv <= self.max_biodiv && r_carbon <= self.max_carbon && r_neuro <= self.max_neuro
    }
}
