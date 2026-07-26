// filename: prometheus-praxis/spine/src/lib.rs

//! Prometheus‑Praxis KER–Lyapunov spine crate.
//!
//! This crate codifies the non‑negotiable eco‑machine spine:
//! - `RiskCoord`: single normalized risk coordinate r_j ∈ [0,1].
//! - `RiskVector`: per‑plane aggregation of coordinates.
//! - `LyapunovWeights`: per‑plane weights w_j with non‑offsettable flags.
//! - `Residual`: Lyapunov energy V_t = ∑_j w_j r_j^2.
//! - `SafeStepGate`: gatekeeper enforcing ΔV_t ≤ 0 and corridor grammar.
//! - Corridor mapping helpers for harmful/beneficial metrics.
//!
//! It is deliberately minimal and self‑contained so all domain planes
//! (hydrology, topology, microplastics, neurorights, Tree‑of‑Life)
//! can plug in without modifying the residual kernel or invariants.

#![forbid(unsafe_code)]
#![deny(missing_docs)]
#![deny(clippy::unwrap_used)]
#![deny(clippy::expect_used)]
#![deny(clippy::panic)]

use serde::{Deserialize, Serialize};
use std::fmt;

/// Identifier for a risk plane (e.g., "hydrology", "topology", "microplastics").
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct PlaneId(pub String);

/// Corridor band for a metric: SAFE, GOLD, HARD.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum CorridorBand {
    /// Safe operating corridor.
    Safe,
    /// Gold corridor; acceptable but monitored.
    Gold,
    /// Hard corridor; near failure thresholds; forbidden for new builds.
    Hard,
}

/// Single risk coordinate r_j ∈ [0,1] attached to a plane and metric name.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct RiskCoord {
    /// Plane identifier this coordinate belongs to.
    pub plane: PlaneId,
    /// Metric name (e.g., "nitrate_ppm", "cognitive_interference_index").
    pub metric: String,
    /// Normalized risk value r_j ∈ [0,1].
    pub value: f64,
}

impl RiskCoord {
    /// Construct a risk coordinate from raw domain units and a mapping function.
    ///
    /// The `map_to_risk` closure must map the raw value into [0,1] according to
    /// the corridor grammar; this helper clamps defensively.
    pub fn from_raw<F>(
        plane: PlaneId,
        metric: impl Into<String>,
        raw_value: f64,
        map_to_risk: F,
    ) -> Self
    where
        F: Fn(f64) -> f64,
    {
        let mut r = map_to_risk(raw_value);
        if r < 0.0 {
            r = 0.0;
        } else if r > 1.0 {
            r = 1.0;
        }
        RiskCoord {
            plane,
            metric: metric.into(),
            value: r,
        }
    }

    /// Map this coordinate into a corridor band given safe/gold thresholds in risk space.
    pub fn corridor(&self, safe_max: f64, gold_max: f64) -> CorridorBand {
        debug_assert!(safe_max >= 0.0 && safe_max <= 1.0);
        debug_assert!(gold_max >= safe_max && gold_max <= 1.0);
        if self.value <= safe_max {
            CorridorBand::Safe
        } else if self.value <= gold_max {
            CorridorBand::Gold
        } else {
            CorridorBand::Hard
        }
    }
}

/// Aggregated risk state for a single plane j at time t.
/// All coordinates MUST share the same PlaneId.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct RiskVector {
    /// Plane identifier.
    pub plane: PlaneId,
    /// Risk coordinates for the plane.
    pub coords: Vec<RiskCoord>,
}

impl RiskVector {
    /// Construct a RiskVector and enforce plane consistency and bounds.
    pub fn new(plane: PlaneId, coords: Vec<RiskCoord>) -> Result<Self, SpineError> {
        for c in &coords {
            if c.plane != plane {
                return Err(SpineError::PlaneMismatch);
            }
            if c.value < 0.0 || c.value > 1.0 {
                return Err(SpineError::RiskOutOfBounds);
            }
        }
        Ok(RiskVector { plane, coords })
    }

    /// Maximum coordinate value r_max for this plane.
    pub fn max_risk(&self) -> f64 {
        self.coords
            .iter()
            .map(|c| c.value)
            .fold(0.0, |acc, v| acc.max(v))
    }
}

/// Lyapunov weight for a plane j: w_j ≥ 0, with non‑offsettable flag.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct LyapunovWeight {
    /// Plane identifier.
    pub plane: PlaneId,
    /// Weight w_j ≥ 0.
    pub weight: f64,
    /// True if plane is governance‑locked (non‑offsettable).
    pub non_offsettable: bool,
}

impl LyapunovWeight {
    /// Create Lyapunov weight; rejects negative weights.
    pub fn new(plane: PlaneId, weight: f64, non_offsettable: bool) -> Result<Self, SpineError> {
        if weight < 0.0 {
            return Err(SpineError::NegativeWeight);
        }
        Ok(LyapunovWeight {
            plane,
            weight,
            non_offsettable,
        })
    }
}

/// Collection of Lyapunov weights for all configured planes.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct LyapunovWeights {
    /// Raw weights per plane.
    pub raw: Vec<LyapunovWeight>,
}

impl LyapunovWeights {
    /// Construct weights, verifying non‑negativity and non‑empty.
    pub fn new(raw: Vec<LyapunovWeight>) -> Result<Self, SpineError> {
        if raw.is_empty() {
            return Err(SpineError::EmptyWeights);
        }
        if raw.iter().any(|w| w.weight < 0.0) {
            return Err(SpineError::NegativeWeight);
        }
        Ok(LyapunovWeights { raw })
    }

    /// Normalized weights: \tilde{w}_i = w_i / ∑_j w_j, preserving Lyapunov grammar.
    pub fn normalized(&self) -> Vec<(PlaneId, f64, bool)> {
        let sum: f64 = self.raw.iter().map(|w| w.weight).sum();
        let denom = if sum == 0.0 { 1.0 } else { sum };
        self.raw
            .iter()
            .map(|w| (w.plane.clone(), w.weight / denom, w.non_offsettable))
            .collect()
    }

    /// Normalized weight for a given plane; returns 0 when missing.
    pub fn normalized_for(&self, plane: &PlaneId) -> f64 {
        let norm = self.normalized();
        norm.into_iter()
            .find(|(pid, _, _)| pid == plane)
            .map(|(_, w, _)| w)
            .unwrap_or(0.0)
    }

    /// Check whether a plane is non‑offsettable.
    pub fn is_non_offsettable(&self, plane: &PlaneId) -> bool {
        self.raw
            .iter()
            .find(|w| &w.plane == plane)
            .map(|w| w.non_offsettable)
            .unwrap_or(false)
    }
}

/// Total Lyapunov energy V_t = ∑_j w_j r_j^2 at time t.
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct Residual {
    /// Energy value V_t ≥ 0.
    pub vt: f64,
}

impl Residual {
    /// Compute V_t from plane risk vectors and Lyapunov weights.
    pub fn compute(vectors: &[RiskVector], weights: &LyapunovWeights) -> Result<Self, SpineError> {
        let mut vt = 0.0;
        for v in vectors {
            let wj = weights.normalized_for(&v.plane);
            if wj < 0.0 {
                return Err(SpineError::NegativeWeight);
            }
            let plane_energy: f64 = v.coords.iter().map(|c| c.value * c.value).sum();
            vt += wj * plane_energy;
        }
        if vt < 0.0 {
            return Err(SpineError::NegativeResidual);
        }
        Ok(Residual { vt })
    }

    /// ΔV_t = V_{t+1} − V_t.
    pub fn delta(next: Residual, current: Residual) -> f64 {
        next.vt - current.vt
    }
}

/// Decision from the SafeStepGate: Allowed or Rejected with a reason.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum SafeStepDecision {
    /// Step is Lyapunov‑safe and corridor‑safe.
    Allowed,
    /// Step violates invariants.
    Rejected(SpineError),
}

/// SafeStepGate: gatekeeper enforcing r_j bounds, corridor grammar, and ΔV_t ≤ 0.
pub struct SafeStepGate;

impl SafeStepGate {
    /// Evaluate a proposed transition from current to next risk vectors.
    ///
    /// Invariants:
    /// - r_j ∈ [0,1] for all coordinates
    /// - non‑offsettable planes must not increase risk when `lock_non_offsettable` is true
    /// - corridor grammar: HARD band forbidden (no‑corridor‑no‑build)
    /// - Lyapunov: V_{t+1} ≤ V_t
    pub fn evaluate_step(
        current_vectors: &[RiskVector],
        next_vectors: &[RiskVector],
        weights: &LyapunovWeights,
        safe_max: f64,
        gold_max: f64,
        lock_non_offsettable: bool,
    ) -> SafeStepDecision {
        if let Err(e) = Self::check_vectors(current_vectors) {
            return SafeStepDecision::Rejected(e);
        }
        if let Err(e) = Self::check_vectors(next_vectors) {
            return SafeStepDecision::Rejected(e);
        }
        if lock_non_offsettable {
            if let Err(e) = Self::check_non_offsettable_planes(current_vectors, next_vectors, weights)
            {
                return SafeStepDecision::Rejected(e);
            }
        }
        if let Err(e) = Self::check_corridors(next_vectors, safe_max, gold_max) {
            return SafeStepDecision::Rejected(e);
        }

        let vt = match Residual::compute(current_vectors, weights) {
            Ok(r) => r,
            Err(e) => return SafeStepDecision::Rejected(e),
        };
        let vt_next = match Residual::compute(next_vectors, weights) {
            Ok(r) => r,
            Err(e) => return SafeStepDecision::Rejected(e),
        };
        let delta = Residual::delta(vt_next, vt);
        if delta > 0.0 {
            return SafeStepDecision::Rejected(SpineError::UnstableStep { delta });
        }

        SafeStepDecision::Allowed
    }

    fn check_vectors(vectors: &[RiskVector]) -> Result<(), SpineError> {
        for v in vectors {
            for c in &v.coords {
                if c.value < 0.0 || c.value > 1.0 {
                    return Err(SpineError::RiskOutOfBounds);
                }
            }
        }
        Ok(())
    }

    fn check_non_offsettable_planes(
        current: &[RiskVector],
        next: &[RiskVector],
        weights: &LyapunovWeights,
    ) -> Result<(), SpineError> {
        for next_vec in next {
            if weights.is_non_offsettable(&next_vec.plane) {
                if let Some(curr_vec) = current.iter().find(|v| v.plane == next_vec.plane) {
                    let curr_max = curr_vec.max_risk();
                    let next_max = next_vec.max_risk();
                    if next_max > curr_max {
                        return Err(SpineError::NonOffsettablePlaneViolation {
                            plane: next_vec.plane.clone(),
                            from: curr_max,
                            to: next_max,
                        });
                    }
                }
            }
        }
        Ok(())
    }

    fn check_corridors(
        vectors: &[RiskVector],
        safe_max: f64,
        gold_max: f64,
    ) -> Result<(), SpineError> {
        for v in vectors {
            for c in &v.coords {
                let band = c.corridor(safe_max, gold_max);
                if matches!(band, CorridorBand::Hard) {
                    return Err(SpineError::CorridorHardViolation {
                        plane: v.plane.clone(),
                        metric: c.metric.clone(),
                        value: c.value,
                    });
                }
            }
        }
        Ok(())
    }
}

/// Spine‑level errors for invariant violations.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum SpineError {
    /// Risk coordinate outside [0,1].
    RiskOutOfBounds,
    /// Lyapunov weight negative.
    NegativeWeight,
    /// Residual energy negative.
    NegativeResidual,
    /// RiskVector contains mixed planes.
    PlaneMismatch,
    /// No weights configured.
    EmptyWeights,
    /// Proposed step increases V_t (ΔV_t > 0).
    UnstableStep { delta: f64 },
    /// Non‑offsettable plane risk increased.
    NonOffsettablePlaneViolation { plane: PlaneId, from: f64, to: f64 },
    /// HARD corridor violation for a metric.
    CorridorHardViolation { plane: PlaneId, metric: String, value: f64 },
}

impl fmt::Display for SpineError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            SpineError::RiskOutOfBounds => write!(f, "risk coordinate out of [0,1] bounds"),
            SpineError::NegativeWeight => write!(f, "Lyapunov weight cannot be negative"),
            SpineError::NegativeResidual => write!(f, "residual V_t cannot be negative"),
            SpineError::PlaneMismatch => write!(f, "risk vector contains coordinates from multiple planes"),
            SpineError::EmptyWeights => write!(f, "Lyapunov weights set is empty"),
            SpineError::UnstableStep { delta } => {
                write!(f, "unsafe Lyapunov step: ΔV_t = {} > 0", delta)
            }
            SpineError::NonOffsettablePlaneViolation { plane, from, to } => write!(
                f,
                "non‑offsettable plane {:?} risk increased from {} to {}",
                plane, from, to
            ),
            SpineError::CorridorHardViolation { plane, metric, value } => write!(
                f,
                "metric {} on plane {:?} entered HARD corridor with value {}",
                metric, plane, value
            ),
        }
    }
}

impl std::error::Error for SpineError {}

/// Harmful metric corridor mapping: SAFE → 0, GOLD → (0,0.4], HARD → (0.4,1].
pub fn harmful_corridor_map(
    x: f64,
    safe_max: f64,
    gold_max: f64,
    hard_max: f64,
) -> f64 {
    if x <= safe_max {
        0.0
    } else if x <= gold_max {
        let span = gold_max - safe_max;
        let rel = (x - safe_max) / span.max(1e-9);
        0.4 * rel
    } else if x <= hard_max {
        let span = hard_max - gold_max;
        let rel = (x - gold_max) / span.max(1e-9);
        0.4 + 0.6 * rel
    } else {
        1.0
    }
}

/// Beneficial metric corridor mapping: high x is good, risk decreases with x.
pub fn beneficial_corridor_map(
    x: f64,
    safe_min: f64,
    gold_min: f64,
    hard_min: f64,
) -> f64 {
    if x >= safe_min {
        0.0
    } else if x >= gold_min {
        let span = safe_min - gold_min;
        let rel = (safe_min - x) / span.max(1e-9);
        0.4 * rel
    } else if x >= hard_min {
        let span = gold_min - hard_min;
        let rel = (gold_min - x) / span.max(1e-9);
        0.4 + 0.6 * rel
    } else {
        1.0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn risk_coord_clamps_unit_interval() {
        let plane = PlaneId("hydrology".to_string());
        let rc = RiskCoord::from_raw(plane.clone(), "nitrate_ppm", -5.0, |x| x);
        assert_eq!(rc.value, 0.0);

        let rc2 = RiskCoord::from_raw(plane, "nitrate_ppm", 2.0, |_x| 2.0);
        assert_eq!(rc2.value, 1.0);
    }

    #[test]
    fn residual_non_negative_and_gate_blocks_unstable() {
        let hydrology = PlaneId("hydrology".to_string());
        let topology = PlaneId("topology".to_string());

        let w_h = LyapunovWeight::new(hydrology.clone(), 1.0, true).unwrap();
        let w_t = LyapunovWeight::new(topology.clone(), 0.5, false).unwrap();
        let weights = LyapunovWeights::new(vec![w_h, w_t]).unwrap();

        let current_h = RiskVector::new(
            hydrology.clone(),
            vec![RiskCoord {
                plane: hydrology.clone(),
                metric: "nitrate_ppm".to_string(),
                value: 0.2,
            }],
        )
        .unwrap();
        let current_t = RiskVector::new(
            topology.clone(),
            vec![RiskCoord {
                plane: topology.clone(),
                metric: "manifest_drift".to_string(),
                value: 0.3,
            }],
        )
        .unwrap();

        let next_h = RiskVector::new(
            hydrology.clone(),
            vec![RiskCoord {
                plane: hydrology.clone(),
                metric: "nitrate_ppm".to_string(),
                value: 0.5,
            }],
        )
        .unwrap();
        let next_t = RiskVector::new(
            topology.clone(),
            vec![RiskCoord {
                plane: topology.clone(),
                metric: "manifest_drift".to_string(),
                value: 0.35,
            }],
        )
        .unwrap();

        let current_vecs = vec![current_h, current_t];
        let next_vecs = vec![next_h, next_t];

        let vt = Residual::compute(&current_vecs, &weights).unwrap();
        let vt_next = Residual::compute(&next_vecs, &weights).unwrap();
        assert!(vt.vt >= 0.0);
        assert!(vt_next.vt >= 0.0);

        let delta = Residual::delta(vt_next, vt);
        assert!(delta > 0.0);

        let decision = SafeStepGate::evaluate_step(
            &current_vecs,
            &next_vecs,
            &weights,
            0.3,
            0.7,
            true,
        );

        match decision {
            SafeStepDecision::Rejected(SpineError::UnstableStep { .. }) => {}
            other => panic!("expected unstable step rejection, got {:?}", other),
        }
    }
}
