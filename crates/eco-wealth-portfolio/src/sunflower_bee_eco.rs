//! Sunflower + bee eco-wealth helpers.
//!
//! This module defines:
//! - BeeHBScore: a bounded [0,1] habitat/behavior scalar for pollinators.
//! - BeeHBScoreCorridor: corridor bounds loaded from ALN / SQLite.
//! - EcoWealthContribution: per-unit eco-wealth scalar for portfolio logic.
//! - no_corridor_no_act guard: fail closed if no corridor or out-of-bounds.
//!
//! It is pure and non-actuating: no hardware, no network, no global state.

#![forbid(unsafe_code)]

use std::fmt;

/// Bounded habitat / behavior score for pollinators in [0,1].
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct BeeHBScore(f64);

impl BeeHBScore {
    /// Construct a BeeHBScore from a raw scalar, clamping into [0,1].
    pub fn new_clamped(raw: f64) -> Self {
        let v = if raw.is_nan() {
            0.0
        } else if raw < 0.0 {
            0.0
        } else if raw > 1.0 {
            1.0
        } else {
            raw
        };
        BeeHBScore(v)
    }

    /// Underlying scalar in [0,1].
    pub fn value(self) -> f64 {
        self.0
    }
}

/// Corridor parameters for BeeHBScore, derived from BeeHBScoreCorridorV1 ALN.
#[derive(Debug, Clone, PartialEq)]
pub struct BeeHBCorridor {
    pub corridor_id: String,
    /// Minimum acceptable HB score to allow eco-positive action.
    pub hb_min: f64,
    /// Warning threshold for monitoring / soft alarms.
    pub hb_warn: f64,
    /// Logical maximum, typically 1.0.
    pub hb_max: f64,
    /// Risk-of-harm ceiling in [0,1] for bee-related actions.
    pub roh_max: f64,
    /// Relative weight of bees within eco-wealth aggregation.
    pub eco_weight: f64,
    /// Hex anchor for provenance / derivation.
    pub evidence_hex: String,
}

impl BeeHBCorridor {
    /// Validate basic invariants on corridor parameters.
    ///
    /// - 0.0 <= hb_min <= hb_warn <= hb_max <= 1.0
    /// - 0.0 <= roh_max <= 1.0
    /// - eco_weight >= 0.0
    pub fn validate(&self) -> Result<(), BeeEcoError> {
        if !(0.0..=1.0).contains(&self.hb_min)
            || !(0.0..=1.0).contains(&self.hb_warn)
            || !(0.0..=1.0).contains(&self.hb_max)
        {
            return Err(BeeEcoError::InvalidCorridor(
                "hb_min/hb_warn/hb_max must be in [0,1]".into(),
            ));
        }
        if self.hb_min > self.hb_warn || self.hb_warn > self.hb_max {
            return Err(BeeEcoError::InvalidCorridor(
                "must satisfy hb_min <= hb_warn <= hb_max".into(),
            ));
        }
        if !(0.0..=1.0).contains(&self.roh_max) {
            return Err(BeeEcoError::InvalidCorridor(
                "roh_max must be in [0,1]".into(),
            ));
        }
        if self.eco_weight < 0.0 {
            return Err(BeeEcoError::InvalidCorridor(
                "eco_weight must be non-negative".into(),
            ));
        }
        Ok(())
    }
}

/// Per-unit eco-wealth contribution from pollinator health.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct EcoWealthContribution(f64);

impl EcoWealthContribution {
    pub fn value(self) -> f64 {
        self.0
    }
}

/// Error type for bee eco-wealth computations.
#[derive(Debug, Clone, PartialEq)]
pub enum BeeEcoError {
    /// No corridor was provided for this unit: "no corridor, no act".
    MissingCorridor,
    /// Corridor parameters are internally inconsistent.
    InvalidCorridor(String),
    /// HB score violates hard corridor bounds.
    CorridorViolation {
        hb: f64,
        hb_min: f64,
        hb_max: f64,
    },
}

impl fmt::Display for BeeEcoError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            BeeEcoError::MissingCorridor => write!(f, "no BeeHB corridor: no corridor, no act"),
            BeeEcoError::InvalidCorridor(msg) => {
                write!(f, "invalid BeeHB corridor parameters: {msg}")
            }
            BeeEcoError::CorridorViolation { hb, hb_min, hb_max } => write!(
                f,
                "BeeHBScore {hb:.3} outside corridor [{hb_min:.3}, {hb_max:.3}]"
            ),
        }
    }
}

impl std::error::Error for BeeEcoError {}

/// Compute a BeeHBScore from primitive monitors.
///
/// All inputs are normalized into [0,1] before combination:
/// - habitat_band in {Optimal, Adequate, Marginal, Hostile} -> scalar
/// - activity_band in {High, Medium, Low, None} -> scalar
/// - pesticide_free_01 as given
/// - nocturnal_light_pollution_01 as given but inverted
///
/// The combination is a simple weighted mean that you can refine later.
///
/// This function is intentionally pure and local; it does not perform any IO.
pub fn compute_bee_hb_score(
    habitat_band: &str,
    activity_band: &str,
    pesticide_free_01: f64,
    nocturnal_light_pollution_01: f64,
) -> BeeHBScore {
    let h = match habitat_band {
        "Optimal" => 1.0,
        "Adequate" => 0.75,
        "Marginal" => 0.4,
        "Hostile" => 0.0,
        _ => 0.0,
    };
    let a = match activity_band {
        "High" => 1.0,
        "Medium" => 0.7,
        "Low" => 0.3,
        "None" => 0.0,
        _ => 0.0,
    };
    let p = pesticide_free_01.clamp(0.0, 1.0);
    let lp = nocturnal_light_pollution_01.clamp(0.0, 1.0);
    let lp_good = 1.0 - lp;

    // Weights: habitat and activity dominant, then pesticide, then light.
    let num = 0.35 * h + 0.35 * a + 0.2 * p + 0.1 * lp_good;
    let den = 0.35 + 0.35 + 0.2 + 0.1;
    let raw = if den > 0.0 { num / den } else { 0.0 };

    BeeHBScore::new_clamped(raw)
}

/// Lift BeeHBScore into an eco-wealth contribution, with "no corridor, no act".
///
/// - If `corridor` is None: returns Err(MissingCorridor).
/// - If corridor is invalid: Err(InvalidCorridor).
/// - If HB score violates [hb_min, hb_max]: Err(CorridorViolation).
/// - Otherwise: EcoWealthContribution = hb * eco_weight.
///
/// CI can call this for every sunflower unit and assert that:
/// - No error is returned for any scheduled act.
/// - Portfolio-level eco-wealth matches expected aggregates.
pub fn bee_eco_wealth_from_hb(
    hb: BeeHBScore,
    corridor: Option<&BeeHBCorridor>,
) -> Result<EcoWealthContribution, BeeEcoError> {
    let corridor = corridor.ok_or(BeeEcoError::MissingCorridor)?;
    corridor.validate()?;

    let v = hb.value();
    if v < corridor.hb_min || v > corridor.hb_max {
        return Err(BeeEcoError::CorridorViolation {
            hb: v,
            hb_min: corridor.hb_min,
            hb_max: corridor.hb_max,
        });
    }

    let contrib = v * corridor.eco_weight;
    Ok(EcoWealthContribution(contrib))
}

/// Aggregate eco-wealth contributions across many sunflower units.
///
/// This is a simple sum; higher-level portfolio logic can normalize or
/// rescale according to total capacity, region, or other weights.
pub fn aggregate_bee_eco_wealth(contribs: &[EcoWealthContribution]) -> f64 {
    contribs.iter().map(|c| c.value()).sum()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample_corridor() -> BeeHBCorridor {
        BeeHBCorridor {
            corridor_id: "BEE-HB-CORRIDOR-2026V1".to_string(),
            hb_min: 0.6,
            hb_warn: 0.75,
            hb_max: 1.0,
            roh_max: 0.30,
            eco_weight: 1.5,
            evidence_hex: "0xBEEHB2026".to_string(),
        }
    }

    #[test]
    fn hb_score_basic_monotonicity() {
        let good = compute_bee_hb_score("Optimal", "High", 1.0, 0.0);
        let bad = compute_bee_hb_score("Hostile", "None", 0.0, 1.0);
        assert!(good.value() > bad.value());
        assert!(good.value() <= 1.0);
        assert!(bad.value() >= 0.0);
    }

    #[test]
    fn no_corridor_no_act_is_enforced() {
        let hb = BeeHBScore::new_clamped(0.8);
        let res = bee_eco_wealth_from_hb(hb, None);
        assert!(matches!(res, Err(BeeEcoError::MissingCorridor)));
    }

    #[test]
    fn corridor_violation_is_caught() {
        let corridor = sample_corridor();
        let hb_low = BeeHBScore::new_clamped(0.3);
        let res = bee_eco_wealth_from_hb(hb_low, Some(&corridor));
        assert!(matches!(
            res,
            Err(BeeEcoError::CorridorViolation { .. })
        ));
    }

    #[test]
    fn eco_wealth_scales_with_hb_and_weight() {
        let corridor = sample_corridor();
        let hb = BeeHBScore::new_clamped(0.8);
        let contrib = bee_eco_wealth_from_hb(hb, Some(&corridor)).expect("ok");
        // Expected = hb * eco_weight.
        assert!((contrib.value() - 0.8 * corridor.eco_weight).abs() < 1e-9);
    }

    #[test]
    fn aggregate_sums_contributions() {
        let cs = [
            EcoWealthContribution(0.5),
            EcoWealthContribution(1.0),
            EcoWealthContribution(0.25),
        ];
        let total = aggregate_bee_eco_wealth(&cs);
        assert!((total - 1.75).abs() < 1e-9);
    }
}
