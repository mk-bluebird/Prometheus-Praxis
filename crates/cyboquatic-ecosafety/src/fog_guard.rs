//! FOG guard: safestep-only executable gate for routing and sewer actuation.
//!
//! This module defines a non-actuating guard that consumes ecosafety
//! diagnostics (corridor, Lyapunov, KER) and produces a safestep verdict
//! used as the sole executable gate for FOG routing and sewer actions.

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

use crate::{LyapunovResidual, RiskCoord, RiskVector};

/// High-level verdict for a candidate FOG / sewer step.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
pub enum FogGuardVerdict {
    Allow,
    Stop,
}

/// Input snapshot for a single candidate step.
/// This is designed to be constructed from existing ecosafety envelopes.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct FogGuardInput {
    /// Per-plane risk coordinates (energy, hydraulics, biology, carbon, materials, biodiversity).
    pub risk: RiskVector,
    /// Lyapunov residual V_{t+1} - V_t.
    pub residual: LyapunovResidual,
    /// Corridor presence for this node and lane.
    pub corridor_present: bool,
    /// Cached safestep flag from upstream ecosafety envelope (if any).
    pub safestep_ok: bool,
    /// KER window summary in 0..1.
    pub k: RiskCoord,
    pub e: RiskCoord,
    pub r: RiskCoord,
}

/// Hard corridor bands for risk coordinates and residual.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct FogGuardBands {
    /// Maximum allowable RoH scalar (0..1).
    pub roh_ceiling: f64,
    /// Maximum allowable Lyapunov residual (non-increase with slack).
    pub residual_max: f64,
}

impl FogGuardBands {
    pub fn default() -> Self {
        Self {
            roh_ceiling: 0.30,
            residual_max: 0.0,
        }
    }
}

/// KER thresholds for deployability.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct FogGuardKerThresholds {
    /// Minimum knowledge factor K.
    pub k_min: f64,
    /// Minimum eco-impact factor E.
    pub e_min: f64,
    /// Maximum risk-of-harm R.
    pub r_max: f64,
}

impl FogGuardKerThresholds {
    pub fn default() -> Self {
        Self {
            k_min: 0.90,
            e_min: 0.90,
            r_max: 0.20,
        }
    }
}

/// Static configuration for the FOG guard.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct FogGuardConfig {
    pub bands: FogGuardBands,
    pub ker: FogGuardKerThresholds,
}

impl FogGuardConfig {
    pub fn default() -> Self {
        Self {
            bands: FogGuardBands::default(),
            ker: FogGuardKerThresholds::default(),
        }
    }
}

/// Core guard that evaluates a candidate step and returns an executable verdict.
/// This is the only place where upstream planning and lane promotion are allowed
/// to influence FOG / sewer actuation.
#[derive(Clone, Debug)]
pub struct FogGuard {
    cfg: FogGuardConfig,
}

impl FogGuard {
    pub fn new(cfg: FogGuardConfig) -> Self {
        Self { cfg }
    }

    /// Convenience constructor with default bands and KER thresholds.
    pub fn with_defaults() -> Self {
        Self {
            cfg: FogGuardConfig::default(),
        }
    }

    pub fn config(&self) -> &FogGuardConfig {
        &self.cfg
    }

    /// Evaluate a single candidate step.
    ///
    /// Invariants enforced:
    /// - No corridor => Stop.
    /// - Lyapunov non-increase with residual <= residual_max.
    /// - RoH (rriskofharm) <= roh_ceiling.
    /// - K >= k_min, E >= e_min, R <= r_max.
    /// - Upstream safestep_ok must be true; if it is false, Stop.
    pub fn evaluate(&self, input: &FogGuardInput) -> FogGuardVerdict {
        if !input.corridor_present {
            return FogGuardVerdict::Stop;
        }

        let roh = input.r.value;
        if !roh.is_finite() || roh > self.cfg.bands.roh_ceiling {
            return FogGuardVerdict::Stop;
        }

        let dv = input.residual.value;
        if !dv.is_finite() || dv > self.cfg.bands.residual_max {
            return FogGuardVerdict::Stop;
        }

        let k = input.k.value;
        let e = input.e.value;
        let r = input.r.value;

        if !(k >= self.cfg.ker.k_min && e >= self.cfg.ker.e_min && r <= self.cfg.ker.r_max) {
            return FogGuardVerdict::Stop;
        }

        if !input.safestep_ok {
            return FogGuardVerdict::Stop;
        }

        FogGuardVerdict::Allow
    }
}
