// filename: ecorestorationshard/air_globe_espd/src/lib.rs
// destination: ecorestorationshard/air_globe_espd/src/lib.rs

#![cfg_attr(feature = "no_std", no_std)]
#![forbid(unsafe_code)]
#![deny(missing_docs)]
#![deny(clippy::unwrap_used)]
#![deny(clippy::expect_used)]
#![deny(clippy::panic)]

//! Air‑globe ESPD core kernels, integrity checking, and composite ecosafety frames.
//!
//! Features:
//! - `no_std` core for ESPD thresholds and integrity checks.
//! - Optional `std` for serde, logging hooks, and Bayesian tuning.
//! - `IntegrityCheckFrame` for adversarial / impossible inputs.
//! - `CompositeFrame` combining air‑globe ESPD and cyboquatic ecosafety.
//! - Bayesian ESPD threshold updates with corridor‑tightening only.

extern crate alloc;

use alloc::vec::Vec;

#[cfg(feature = "std")]
extern crate std;

#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};

/// Normalized risk/probability scalar in `[0,1]`.
pub type Scalar = f32;

/// Integrity violation kinds for incoming measurements or diagnostics.
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum IntegrityViolationKind {
    /// Any value < 0 for a coordinate intended to be normalized.
    NegativeNormalized,
    /// Any value > 1 for a coordinate intended to be normalized.
    AboveOneNormalized,
    /// Value far outside the configured corridor range.
    OutOfCorridor,
}

/// Integrity violation record for audit and governance.
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Clone)]
pub struct IntegrityViolation {
    /// Node identifier or sensor id.
    pub node_id: alloc::string::String,
    /// Coordinate label e.g., "espd_risk", "ecosafety_r".
    pub coord: alloc::string::String,
    /// The offending numeric value.
    pub value: f32,
    /// Expected min corridor bound, if available.
    pub corridor_min: Option<f32>,
    /// Expected max corridor bound, if available.
    pub corridor_max: Option<f32>,
    /// Violation kind.
    pub kind: IntegrityViolationKind,
}

/// Integrity check configuration for a single coordinate.
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Clone, Copy)]
pub struct CoordCorridor {
    /// Whether the value is normalized and must lie in `[0,1]`.
    pub normalized: bool,
    /// Optional corridor minimum (inclusive) in physical units or normalized space.
    pub corridor_min: Option<f32>,
    /// Optional corridor maximum (inclusive).
    pub corridor_max: Option<f32>,
}

/// Cheap integrity frame to pre‑screen inputs before heavy kernels.
///
/// This is deliberately simple and can run under `no_std`.
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Clone)]
pub struct IntegrityCheckFrame {
    /// Corridor spec for ESPD risk coordinate.
    pub espd_risk: CoordCorridor,
    /// Corridor spec for ecosafety risk coordinate.
    pub ecosafety_risk: CoordCorridor,
    /// Corridor spec for biodiversity risk coordinate.
    pub biodiversity_risk: CoordCorridor,
}

impl Default for IntegrityCheckFrame {
    fn default() -> Self {
        Self {
            espd_risk: CoordCorridor {
                normalized: true,
                corridor_min: Some(0.0),
                corridor_max: Some(1.0),
            },
            ecosafety_risk: CoordCorridor {
                normalized: true,
                corridor_min: Some(0.0),
                corridor_max: Some(1.0),
            },
            biodiversity_risk: CoordCorridor {
                normalized: true,
                corridor_min: Some(0.0),
                corridor_max: Some(1.0),
            },
        }
    }
}

impl IntegrityCheckFrame {
    /// Check a single coordinate value against its corridor.
    fn check_coord(
        &self,
        node_id: &str,
        label: &str,
        v: f32,
        spec: CoordCorridor,
        out: &mut Vec<IntegrityViolation>,
    ) {
        if spec.normalized {
            if v < 0.0 {
                out.push(IntegrityViolation {
                    node_id: alloc::string::String::from(node_id),
                    coord: alloc::string::String::from(label),
                    value: v,
                    corridor_min: spec.corridor_min,
                    corridor_max: spec.corridor_max,
                    kind: IntegrityViolationKind::NegativeNormalized,
                });
                return;
            }
            if v > 1.0 {
                out.push(IntegrityViolation {
                    node_id: alloc::string::String::from(node_id),
                    coord: alloc::string::String::from(label),
                    value: v,
                    corridor_min: spec.corridor_min,
                    corridor_max: spec.corridor_max,
                    kind: IntegrityViolationKind::AboveOneNormalized,
                });
                return;
            }
        }

        if let Some(lo) = spec.corridor_min {
            if v < lo {
                out.push(IntegrityViolation {
                    node_id: alloc::string::String::from(node_id),
                    coord: alloc::string::String::from(label),
                    value: v,
                    corridor_min: spec.corridor_min,
                    corridor_max: spec.corridor_max,
                    kind: IntegrityViolationKind::OutOfCorridor,
                });
                return;
            }
        }
        if let Some(hi) = spec.corridor_max {
            if v > hi {
                out.push(IntegrityViolation {
                    node_id: alloc::string::String::from(node_id),
                    coord: alloc::string::String::from(label),
                    value: v,
                    corridor_min: spec.corridor_min,
                    corridor_max: spec.corridor_max,
                    kind: IntegrityViolationKind::OutOfCorridor,
                });
            }
        }
    }

    /// Run integrity checks on a single composite sample.
    ///
    /// Returns a list of violations; empty means the window is acceptable
    /// for downstream heavy computation.
    pub fn check_sample(
        &self,
        node_id: &str,
        espd_risk: Scalar,
        ecosafety_risk: Scalar,
        biodiversity_risk: Scalar,
    ) -> Vec<IntegrityViolation> {
        let mut out = Vec::new();
        self.check_coord(node_id, "espd_risk", espd_risk, self.espd_risk, &mut out);
        self.check_coord(
            node_id,
            "ecosafety_risk",
            ecosafety_risk,
            self.ecosafety_risk,
            &mut out,
        );
        self.check_coord(
            node_id,
            "biodiversity_risk",
            biodiversity_risk,
            self.biodiversity_risk,
            &mut out,
        );
        out
    }
}

/// ESPD deployment state classification.
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EspdState {
    /// Safe to deploy.
    Deployable,
    /// Allowed for piloting / limited experiments.
    PilotOnly,
    /// Forbidden under current corridors.
    Forbidden,
}

/// ESPD thresholds with explicit corridor governance.
///
/// These thresholds are subject to Bayesian updating but cannot be widened
/// without explicit governance approval.
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Clone)]
pub struct EspdThresholds {
    /// Max allowed ESPD risk for Deployable classification.
    pub deployable_max: Scalar,
    /// Max allowed ESPD risk for PilotOnly; above this is Forbidden.
    pub pilot_max: Scalar,
    /// Corridor minimum (usually 0.0).
    pub corridor_min: Scalar,
    /// Corridor maximum (usually 1.0).
    pub corridor_max: Scalar,
}

impl EspdThresholds {
    /// Classify a risk value under the current thresholds.
    pub fn classify(&self, espd_risk: Scalar) -> EspdState {
        if espd_risk <= self.deployable_max {
            EspdState::Deployable
        } else if espd_risk <= self.pilot_max {
            EspdState::PilotOnly
        } else {
            EspdState::Forbidden
        }
    }
}

/// Simple Bayesian updater for ESPD thresholds using Beta‑like evidence.
///
/// This is intentionally minimal; more complex priors can be wrapped around it.
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Clone)]
pub struct EspdBayesConfig {
    /// Pseudo‑count for "safe" observations.
    pub alpha_safe: f64,
    /// Pseudo‑count for "unsafe" observations.
    pub beta_unsafe: f64,
    /// Maximum allowed widening of deployable_max upward.
    pub max_deployable_relax: f32,
    /// Maximum allowed widening of pilot_max upward.
    pub max_pilot_relax: f32,
}

/// Result of a Bayesian update, before governance gating.
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Clone)]
pub struct EspdBayesProposal {
    /// Proposed new thresholds.
    pub proposed: EspdThresholds,
    /// Whether the proposal tightens corridors (true) or attempts to widen (false).
    pub tightens: bool,
}

impl EspdBayesConfig {
    /// Propose updated thresholds from data, without applying governance checks.
    ///
    /// `n_safe` and `n_unsafe` are counts of observed ESPD outcomes consistent
    /// with current classification rules.
    pub fn propose(
        &self,
        current: &EspdThresholds,
        n_safe: u64,
        n_unsafe: u64,
    ) -> EspdBayesProposal {
        let a = self.alpha_safe + (n_safe as f64);
        let b = self.beta_unsafe + (n_unsafe as f64);

        // Conservative posterior mean of failure rate.
        let failure_rate = b / (a + b);
        let safe_rate = 1.0 - failure_rate;

        // Map safe_rate into tighter thresholds in `[corridor_min, corridor_max]`.
        let deployable_max = (current.corridor_min
            + (current.corridor_max - current.corridor_min) * (safe_rate as f32))
            .min(current.deployable_max);

        let pilot_max = current
            .pilot_max
            .min(current.corridor_max - (failure_rate as f32) * (current.corridor_max - current.corridor_min));

        let proposed = EspdThresholds {
            deployable_max,
            pilot_max,
            corridor_min: current.corridor_min,
            corridor_max: current.corridor_max,
        };

        let tightens = proposed.deployable_max <= current.deployable_max
            && proposed.pilot_max <= current.pilot_max;

        EspdBayesProposal { proposed, tightens }
    }

    /// Apply governance: accept only proposals that do not widen corridors unless explicitly allowed.
    pub fn apply_with_governance(
        &self,
        current: &EspdThresholds,
        proposal: EspdBayesProposal,
        allow_relax: bool,
    ) -> EspdThresholds {
        if proposal.tightens {
            return proposal.proposed;
        }

        if !allow_relax {
            // Governance forbids widening; keep current thresholds.
            return current.clone();
        }

        // If relaxation is allowed, cap it by configured maxima.
        let max_deploy = current.deployable_max + self.max_deployable_relax;
        let max_pilot = current.pilot_max + self.max_pilot_relax;

        EspdThresholds {
            deployable_max: proposal.proposed.deployable_max.min(max_deploy),
            pilot_max: proposal.proposed.pilot_max.min(max_pilot),
            corridor_min: current.corridor_min,
            corridor_max: current.corridor_max,
        }
    }
}

/// Per‑node ESPD diagnostics summary.
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Clone)]
pub struct EspdNodeSummary {
    /// ESPD risk scalar in `[0,1]`.
    pub espd_risk: Scalar,
    /// ESPD classification under current thresholds.
    pub state: EspdState,
    /// Optional auxiliary benefit scalar (e.g., climate benefit).
    pub benefit: Option<Scalar>,
}

/// Per‑node ecosafety diagnostics summary.
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Clone)]
pub struct EcoSafetyNodeSummary {
    /// Ecosafety risk scalar in `[0,1]`.
    pub ecosafety_risk: Scalar,
    /// Biodiversity risk scalar in `[0,1]`.
    pub biodiversity_risk: Scalar,
    /// Distance to ecosafety corridor center (e.g., Lyapunov residual slice).
    pub eco_distance: Option<Scalar>,
}

/// Unified composite frame combining air‑globe ESPD and cyboquatic ecosafety.
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Clone)]
pub struct CompositeFrame {
    /// Node identifier.
    pub node_id: alloc::string::String,
    /// UTC time window start.
    pub ts_start_utc: alloc::string::String,
    /// UTC time window end.
    pub ts_end_utc: alloc::string::String,
    /// ESPD summary.
    pub espd: EspdNodeSummary,
    /// Ecosafety/biodiversity summary.
    pub ecosafety: EcoSafetyNodeSummary,
}

impl CompositeFrame {
    /// Compute a unified shard‑update‑style record suitable for EcoNet ledger.
    pub fn to_shard_update(&self) -> CompositeShardUpdate {
        CompositeShardUpdate {
            node_id: self.node_id.clone(),
            ts_start_utc: self.ts_start_utc.clone(),
            ts_end_utc: self.ts_end_utc.clone(),
            espd_risk: self.espd.espd_risk,
            espd_state: self.espd.state,
            espd_benefit: self.espd.benefit,
            ecosafety_risk: self.ecosafety.ecosafety_risk,
            biodiversity_risk: self.ecosafety.biodiversity_risk,
            eco_distance: self.ecosafety.eco_distance,
        }
    }
}

/// Ledger‑ready composite shard update for ESPD + ecosafety.
///
/// This is intended to map into qpudatashard rows.
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Clone)]
pub struct CompositeShardUpdate {
    /// Node identifier.
    pub node_id: alloc::string::String,
    /// Time window start.
    pub ts_start_utc: alloc::string::String,
    /// Time window end.
    pub ts_end_utc: alloc::string::String,
    /// ESPD risk scalar.
    pub espd_risk: Scalar,
    /// ESPD state.
    pub espd_state: EspdState,
    /// Optional ESPD benefit.
    pub espd_benefit: Option<Scalar>,
    /// Ecosafety risk.
    pub ecosafety_risk: Scalar,
    /// Biodiversity risk.
    pub biodiversity_risk: Scalar,
    /// Distance to ecosafety corridor center.
    pub eco_distance: Option<Scalar>,
}

// --- Optional std-only bindings for wasm/Android FFI ---

#[cfg(all(feature = "std", feature = "wasm-bindgen"))]
pub mod wasm_bindings {
    use super::*;
    use wasm_bindgen::prelude::*;

    /// Minimal struct exposed to JavaScript for ESPD classification.
    #[wasm_bindgen]
    pub struct JsEspdThresholds {
        inner: EspdThresholds,
    }

    #[wasm_bindgen]
    impl JsEspdThresholds {
        #[wasm_bindgen(constructor)]
        pub fn new(deployable_max: f32, pilot_max: f32) -> JsEspdThresholds {
            JsEspdThresholds {
                inner: EspdThresholds {
                    deployable_max,
                    pilot_max,
                    corridor_min: 0.0,
                    corridor_max: 1.0,
                },
            }
        }

        #[wasm_bindgen]
        pub fn classify(&self, espd_risk: f32) -> String {
            match self.inner.classify(espd_risk) {
                EspdState::Deployable => "DEPLOYABLE".to_string(),
                EspdState::PilotOnly => "PILOT_ONLY".to_string(),
                EspdState::Forbidden => "FORBIDDEN".to_string(),
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn integrity_frame_flags_impossible_values() {
        let frame = IntegrityCheckFrame::default();
        let violations = frame.check_sample("node-1", -0.1, 1.2, 0.5);
        assert!(!violations.is_empty());
        assert!(violations.iter().any(|v| v.coord == "espd_risk"));
        assert!(violations.iter().any(|v| v.coord == "ecosafety_risk"));
    }

    #[test]
    fn espd_thresholds_classify_states() {
        let t = EspdThresholds {
            deployable_max: 0.2,
            pilot_max: 0.5,
            corridor_min: 0.0,
            corridor_max: 1.0,
        };
        assert_eq!(t.classify(0.1), EspdState::Deployable);
        assert_eq!(t.classify(0.3), EspdState::PilotOnly);
        assert_eq!(t.classify(0.8), EspdState::Forbidden);
    }

    #[test]
    fn bayes_proposal_tightens_by_default() {
        let cfg = EspdBayesConfig {
            alpha_safe: 1.0,
            beta_unsafe: 1.0,
            max_deployable_relax: 0.05,
            max_pilot_relax: 0.10,
        };
        let current = EspdThresholds {
            deployable_max: 0.3,
            pilot_max: 0.6,
            corridor_min: 0.0,
            corridor_max: 1.0,
        };
        let proposal = cfg.propose(&current, 100, 5);
        assert!(proposal.tightens);
        let updated = cfg.apply_with_governance(&current, proposal, false);
        assert!(updated.deployable_max <= current.deployable_max);
        assert!(updated.pilot_max <= current.pilot_max);
    }

    #[test]
    fn composite_frame_to_shard_update_roundtrip() {
        let cf = CompositeFrame {
            node_id: "node-xyz".into(),
            ts_start_utc: "2026-07-07T00:00:00Z".into(),
            ts_end_utc: "2026-07-07T00:05:00Z".into(),
            espd: EspdNodeSummary {
                espd_risk: 0.25,
                state: EspdState::Deployable,
                benefit: Some(0.8),
            },
            ecosafety: EcoSafetyNodeSummary {
                ecosafety_risk: 0.3,
                biodiversity_risk: 0.4,
                eco_distance: Some(0.1),
            },
        };
        let upd = cf.to_shard_update();
        assert_eq!(upd.node_id, "node-xyz");
        assert_eq!(upd.espd_risk, 0.25);
        assert_eq!(upd.ecosafety_risk, 0.3);
        assert_eq!(upd.biodiversity_risk, 0.4);
    }
}
