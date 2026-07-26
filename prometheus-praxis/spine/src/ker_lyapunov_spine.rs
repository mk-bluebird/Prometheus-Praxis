// filename: prometheus-praxis/spine/src/ker_lyapunov_spine.rs

//! KER–Lyapunov foundational spine for Prometheus‑Praxis.
//! This module codifies RiskCoord, RiskVector, LyapunovWeights, Residual,
//! SafeStepGate, NonOffsettablePlanes, and corridor grammar in a single,
//! verifiable Rust crate (edition 2024, rust-version = "1.85").
//!
//! All invariants are non‑negotiable:
//! - r_j ∈ [0,1] for every risk coordinate
//! - V_t ≥ 0 for all times t
//! - safe step requires V_{t+1} ≤ V_t
//! - non‑offsettable planes cannot be modified by normal governance actions

#![forbid(unsafe_code)]
#![deny(missing_docs)]
#![deny(clippy::unwrap_used)]
#![deny(clippy::expect_used)]
#![deny(clippy::panic)]

use std::fmt;

/// Identifier for a risk plane (carbon, biodiversity, hydrology, topology, identity, …).
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct PlaneId(pub String);

/// Corridor band for a given metric on a plane: SAFE, GOLD, HARD.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CorridorBand {
    /// Safe operating band; desired long‑term corridor.
    Safe,
    /// Gold corridor; acceptable but monitored.
    Gold,
    /// Hard corridor; approaching or at failure thresholds.
    Hard,
}

/// Single risk coordinate r_j ∈ [0,1] attached to a plane and a metric name.
#[derive(Debug, Clone, PartialEq)]
pub struct RiskCoord {
    /// Plane identifier this coordinate belongs to.
    pub plane: PlaneId,
    /// Metric name (e.g., "nitrate_ppm", "surcharge_index", "biodiversity_index").
    pub metric: String,
    /// Normalized risk value r_j ∈ [0,1].
    pub value: f64,
}

impl RiskCoord {
    /// Construct a risk coordinate from a raw value and a corridor mapping function.
    ///
    /// The `map_to_risk` closure must map raw domain units into [0,1], enforcing
    /// the corridor grammar for the metric. This helper clamps results defensively.
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

    /// Return the corridor band for this coordinate given safe/gold/hard thresholds
    /// expressed in normalized risk space.
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
/// All coordinates MUST belong to the same plane.
#[derive(Debug, Clone, PartialEq)]
pub struct RiskVector {
    /// Plane identifier.
    pub plane: PlaneId,
    /// Risk coordinates for that plane.
    pub coords: Vec<RiskCoord>,
}

impl RiskVector {
    /// Construct a RiskVector, enforcing that all coordinates share the same PlaneId.
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

    /// Compute the maximum coordinate value r_max for this plane.
    pub fn max_risk(&self) -> f64 {
        self.coords
            .iter()
            .map(|c| c.value)
            .fold(0.0, |acc, v| acc.max(v))
    }
}

/// Lyapunov weight for a plane j: w_j ≥ 0, with non‑offsettable flag.
#[derive(Debug, Clone, PartialEq)]
pub struct LyapunovWeight {
    /// Plane identifier.
    pub plane: PlaneId,
    /// Weight w_j ≥ 0.
    pub weight: f64,
    /// True if plane is non‑offsettable (constitutionally locked).
    pub non_offsettable: bool,
}

impl LyapunovWeight {
    /// Create a Lyapunov weight; returns error if weight is negative.
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

/// Collection of Lyapunov weights for all configured planes, with normalization.
#[derive(Debug, Clone, PartialEq)]
pub struct LyapunovWeights {
    /// Raw weights per plane.
    pub raw: Vec<LyapunovWeight>,
}

impl LyapunovWeights {
    /// Construct weights from a list of LyapunovWeight, verifying non‑negativity.
    pub fn new(raw: Vec<LyapunovWeight>) -> Result<Self, SpineError> {
        if raw.is_empty() {
            return Err(SpineError::EmptyWeights);
        }
        if raw.iter().any(|w| w.weight < 0.0) {
            return Err(SpineError::NegativeWeight);
        }
        Ok(LyapunovWeights { raw })
    }

    /// Compute normalized Tree‑of‑Life weights: \tilde{w}_i = w_i / ∑_j w_j.
    pub fn normalized(&self) -> Vec<(PlaneId, f64, bool)> {
        let sum: f64 = self.raw.iter().map(|w| w.weight).sum();
        let denom = if sum == 0.0 { 1.0 } else { sum };
        self.raw
            .iter()
            .map(|w| (w.plane.clone(), w.weight / denom, w.non_offsettable))
            .collect()
    }

    /// Lookup normalized weight for a given plane; returns 0 if missing.
    pub fn normalized_for(&self, plane: &PlaneId) -> f64 {
        let norm = self.normalized();
        norm.into_iter()
            .find(|(pid, _, _)| pid == plane)
            .map(|(_, w, _)| w)
            .unwrap_or(0.0)
    }

    /// Check if a plane is non‑offsettable.
    pub fn is_non_offsettable(&self, plane: &PlaneId) -> bool {
        self.raw
            .iter()
            .find(|w| &w.plane == plane)
            .map(|w| w.non_offsettable)
            .unwrap_or(false)
    }
}

/// Total Lyapunov energy V_t = ∑_j w_j r_j^2 over all planes at time t.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Residual {
    /// Energy value V_t ≥ 0.
    pub vt: f64,
}

impl Residual {
    /// Compute V_t from plane risk vectors and Lyapunov weights, using normalized weights.
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
            // Defensive guard; mathematically vt should be ≥ 0.
            return Err(SpineError::NegativeResidual);
        }
        Ok(Residual { vt })
    }

    /// Compute ΔV_t = V_{t+1} − V_t from two residuals.
    pub fn delta(next: Residual, current: Residual) -> f64 {
        next.vt - current.vt
    }
}

/// Safe step gate decision: either the step is allowed or rejected with a reason.
#[derive(Debug, Clone, PartialEq)]
pub enum SafeStepDecision {
    /// Step is Lyapunov‑safe and corridor‑safe; commit is allowed.
    Allowed,
    /// Step is rejected; contains error describing the invariant violation.
    Rejected(SpineError),
}

/// SafeStepGate: omnipresent guardian enforcing r_j bounds, corridor grammar, and ΔV_t ≤ 0.
pub struct SafeStepGate;

impl SafeStepGate {
    /// Evaluate a proposed transition from current to next risk vectors given Lyapunov weights
    /// and corridor thresholds. All invariants are enforced here:
    /// - r_j ∈ [0,1]
    /// - non‑offsettable planes cannot be changed by this call if `lock_non_offsettable` is true
    /// - corridor grammar: no coordinate in HARD band
    /// - Lyapunov: V_{t+1} ≤ V_t
    pub fn evaluate_step(
        current_vectors: &[RiskVector],
        next_vectors: &[RiskVector],
        weights: &LyapunovWeights,
        safe_max: f64,
        gold_max: f64,
        lock_non_offsettable: bool,
    ) -> SafeStepDecision {
        // 1. r_j bounds and non‑offsettable planes invariants.
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

        // 2. Corridor grammar: forbid HARD band transitions.
        if let Err(e) = Self::check_corridors(next_vectors, safe_max, gold_max) {
            return SafeStepDecision::Rejected(e);
        }

        // 3. Lyapunov residual: require V_{t+1} ≤ V_t.
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
                // Find corresponding current vector for this plane.
                if let Some(curr_vec) = current.iter().find(|v| v.plane == next_vec.plane) {
                    // Compare max risk; if next increases risk, we reject.
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
#[derive(Debug, Clone, PartialEq)]
pub enum SpineError {
    /// Risk coordinate has value outside [0,1].
    RiskOutOfBounds,
    /// Lyapunov weight is negative.
    NegativeWeight,
    /// Residual energy is negative (should not occur if math is correct).
    NegativeResidual,
    /// RiskVector contains coordinates from multiple planes.
    PlaneMismatch,
    /// No weights configured.
    EmptyWeights,
    /// Proposed step increases V_t (unstable Lyapunov step).
    UnstableStep { delta: f64 },
    /// Non‑offsettable plane risk increased in proposed step.
    NonOffsettablePlaneViolation { plane: PlaneId, from: f64, to: f64 },
    /// Proposed step enters HARD corridor band.
    CorridorHardViolation { plane: PlaneId, metric: String, value: f64 },
}

impl fmt::Display for SpineError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            SpineError::RiskOutOfBounds => write!(f, "risk coordinate out of [0,1] bounds"),
            SpineError::NegativeWeight => write!(f, "Lyapunov weight cannot be negative"),
            SpineError::NegativeResidual => write!(f, "residual V_t cannot be negative"),
            SpineError::PlaneMismatch => write!(f, "risk vector contains mixed planes"),
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

/// Example corridor mapping function for a harmful metric (increasing harm with value).
///
/// Maps raw x into [0,1] with three bands:
/// - x ≤ safe_max -> r = 0
/// - safe_max < x ≤ gold_max -> r ∈ (0, 0.4]
/// - gold_max < x ≤ hard_max -> r ∈ (0.4, 1]
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

/// Example corridor mapping function for a beneficial metric (decreasing risk with value).
///
/// High x is good, so risk decreases with x; HARD band is low x.
pub fn beneficial_corridor_map(
    x: f64,
    safe_min: f64,
    gold_min: f64,
    hard_min: f64,
) -> f64 {
    // safe_min ≥ gold_min ≥ hard_min, and x ≥ 0.
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
    fn risk_coord_clamps_to_unit_interval() {
        let plane = PlaneId("hydrology".to_string());
        let rc = RiskCoord::from_raw(plane.clone(), "nitrate_ppm", -5.0, |x| x);
        assert_eq!(rc.value, 0.0);

        let rc2 = RiskCoord::from_raw(plane, "nitrate_ppm", 2.0, |_x| 2.0);
        assert_eq!(rc2.value, 1.0);
    }

    #[test]
    fn residual_non_negative_and_safe_step_gate_blocks_unstable_step() {
        let hydrology = PlaneId("hydrology".to_string());
        let topology = PlaneId("topology".to_string());

        let w_hydro = LyapunovWeight::new(hydrology.clone(), 1.0, true).unwrap();
        let w_topo = LyapunovWeight::new(topology.clone(), 0.5, false).unwrap();
        let weights = LyapunovWeights::new(vec![w_hydro, w_topo]).unwrap();

        let current = RiskVector::new(
            hydrology.clone(),
            vec![RiskCoord {
                plane: hydrology.clone(),
                metric: "nitrate_ppm".to_string(),
                value: 0.2,
            }],
        )
        .unwrap();
        let current_topo = RiskVector::new(
            topology.clone(),
            vec![RiskCoord {
                plane: topology.clone(),
                metric: "manifest_drift".to_string(),
                value: 0.3,
            }],
        )
        .unwrap();

        let next = RiskVector::new(
            hydrology.clone(),
            vec![RiskCoord {
                plane: hydrology.clone(),
                metric: "nitrate_ppm".to_string(),
                value: 0.5,
            }],
        )
        .unwrap();
        let next_topo = RiskVector::new(
            topology.clone(),
            vec![RiskCoord {
                plane: topology.clone(),
                metric: "manifest_drift".to_string(),
                value: 0.35,
            }],
        )
        .unwrap();

        let current_vecs = vec![current, current_topo];
        let next_vecs = vec![next, next_topo];

        let vt_current = Residual::compute(&current_vecs, &weights).unwrap();
        let vt_next = Residual::compute(&next_vecs, &weights).unwrap();
        assert!(vt_current.vt >= 0.0);
        assert!(vt_next.vt >= 0.0);

        let delta = Residual::delta(vt_next, vt_current);
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
