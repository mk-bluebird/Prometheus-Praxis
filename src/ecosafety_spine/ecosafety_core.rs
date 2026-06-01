#![no_std]
#![forbid(unsafe_code)]
#![deny(warnings)]

use core::cmp::Ordering;

pub const MAX_ROH: f64 = 0.30;
pub const GOLD_ROH: f64 = 0.13;
pub const SAFE_ROH: f64 = 0.10;
pub const MIN_K_DEPLOYABLE: f64 = 0.90;
pub const MIN_E_DEPLOYABLE: f64 = 0.90;
pub const DEFAULT_EPSILON: f64 = 1e-6;
pub const DEFAULT_WEIGHTS: [f64; 5] = [1.0, 1.2, 1.0, 1.5, 1.1];

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct RiskCoord(pub f64);

impl RiskCoord {
    pub fn new(value: f64) -> Self {
        RiskCoord(value.clamp(0.0, 1.0))
    }
    
    pub fn from_raw(raw: f64) -> Self {
        RiskCoord::new(raw)
    }
    
    pub fn is_safe(self) -> bool {
        self.0 <= SAFE_ROH
    }
    
    pub fn is_gold(self) -> bool {
        self.0 <= GOLD_ROH
    }
    
    pub fn is_hard(self) -> bool {
        self.0 <= MAX_ROH
    }
    
    pub fn value(self) -> f64 {
        self.0
    }
}

impl PartialOrd for RiskCoord {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        self.0.partial_cmp(&other.0)
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CorridorBands {
    pub safe: f64,
    pub gold: f64,
    pub hard: f64,
}

impl CorridorBands {
    pub fn new(safe: f64, gold: f64, hard: f64) -> Option<Self> {
        if safe < gold && gold < hard {
            Some(Self { safe, gold, hard })
        } else {
            None
        }
    }
    
    pub fn validate(&self) -> bool {
        self.safe < self.gold && self.gold < self.hard
    }
    
    pub fn normalize(&self, raw_value: f64) -> RiskCoord {
        if raw_value <= self.safe {
            RiskCoord(0.0)
        } else if raw_value >= self.hard {
            RiskCoord(1.0)
        } else if raw_value <= self.gold {
            let denominator = self.gold - self.safe;
            if denominator.abs() < DEFAULT_EPSILON {
                RiskCoord(0.0)
            } else {
                let t = (raw_value - self.safe) / denominator;
                RiskCoord(0.5 * t)
            }
        } else {
            let denominator = self.hard - self.gold;
            if denominator.abs() < DEFAULT_EPSILON {
                RiskCoord(0.5)
            } else {
                let t = (raw_value - self.gold) / denominator;
                RiskCoord(0.5 + 0.5 * t)
            }
        }
    }
    
    pub fn tighten(&self, delta: f64) -> Option<Self> {
        let delta_clamped = delta.clamp(0.0, 0.15);
        let new_hard = self.hard * (1.0 - delta_clamped);
        if new_hard > self.gold {
            Some(Self {
                safe: self.safe,
                gold: self.gold,
                hard: new_hard,
            })
        } else {
            None
        }
    }
    
    pub fn effective_width(&self) -> f64 {
        self.hard - self.safe
    }
    
    pub fn operational_score(&self, reference: &Self) -> f64 {
        if reference.hard.abs() < DEFAULT_EPSILON {
            1.0
        } else {
            self.hard / reference.hard
        }
    }
}

#[derive(Clone, Debug)]
pub struct RiskVector {
    pub r_energy: RiskCoord,
    pub r_hydraulic: RiskCoord,
    pub r_biology: RiskCoord,
    pub r_carbon: RiskCoord,
    pub r_materials: RiskCoord,
}

impl RiskVector {
    pub fn new(
        r_energy: RiskCoord,
        r_hydraulic: RiskCoord,
        r_biology: RiskCoord,
        r_carbon: RiskCoord,
        r_materials: RiskCoord,
    ) -> Self {
        Self {
            r_energy,
            r_hydraulic,
            r_biology,
            r_carbon,
            r_materials,
        }
    }
    
    pub fn max_risk(&self) -> RiskCoord {
        [
            self.r_energy,
            self.r_hydraulic,
            self.r_biology,
            self.r_carbon,
            self.r_materials,
        ]
        .iter()
        .max_by(|a, b| a.partial_cmp(b).unwrap_or(Ordering::Equal))
        .copied()
        .unwrap_or(RiskCoord(0.0))
    }
    
    pub fn weighted_sum(&self, weights: &[f64; 5]) -> RiskCoord {
        let sum = weights[0] * self.r_energy.0
            + weights[1] * self.r_hydraulic.0
            + weights[2] * self.r_biology.0
            + weights[3] * self.r_carbon.0
            + weights[4] * self.r_materials.0;
        
        let total_weight: f64 = weights.iter().sum();
        if total_weight.abs() < DEFAULT_EPSILON {
            RiskCoord(0.0)
        } else {
            RiskCoord::new(sum / total_weight)
        }
    }
    
    pub fn as_array(&self) -> [RiskCoord; 5] {
        [
            self.r_energy,
            self.r_hydraulic,
            self.r_biology,
            self.r_carbon,
            self.r_materials,
        ]
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct LyapunovResidual(pub f64);

impl LyapunovResidual {
    pub fn from_risk_vector(rv: &RiskVector, weights: &[f64; 5]) -> Self {
        let v = weights[0] * rv.r_energy.0.powi(2)
            + weights[1] * rv.r_hydraulic.0.powi(2)
            + weights[2] * rv.r_biology.0.powi(2)
            + weights[3] * rv.r_carbon.0.powi(2)
            + weights[4] * rv.r_materials.0.powi(2);
        LyapunovResidual(v)
    }
    
    pub fn value(self) -> f64 {
        self.0
    }
    
    pub fn is_decreasing_from(self, previous: Self, epsilon: f64) -> bool {
        self.0 <= previous.0 + epsilon
    }
    
    pub fn margin_from(self, previous: Self) -> f64 {
        previous.0 - self.0
    }
}

pub fn safestep_ok(v_prev: LyapunovResidual, v_next: LyapunovResidual, epsilon: f64) -> bool {
    v_next.is_decreasing_from(v_prev, epsilon)
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct KerTriad {
    pub k: f64,
    pub e: f64,
    pub r: f64,
}

impl KerTriad {
    pub fn new(k: f64, e: f64, r: f64) -> Self {
        Self {
            k: k.clamp(0.0, 1.0),
            e: e.clamp(0.0, 1.0),
            r: r.clamp(0.0, MAX_ROH),
        }
    }
    
    pub fn is_deployable(&self) -> bool {
        self.k >= MIN_K_DEPLOYABLE && self.e >= MIN_E_DEPLOYABLE && self.r <= GOLD_ROH
    }
    
    pub fn is_staging(&self) -> bool {
        self.k >= 0.85 && self.e >= 0.85 && self.r <= 0.20
    }
    
    pub fn is_research(&self) -> bool {
        !self.is_deployable() && !self.is_staging()
    }
    
    pub fn lane(&self) -> Lane {
        if self.is_deployable() {
            Lane::Production
        } else if self.is_staging() {
            Lane::Staging
        } else {
            Lane::Research
        }
    }
    
    pub fn eco_wealth(&self) -> f64 {
        (self.k * self.e * (1.0 - self.r)).clamp(0.0, 1.0)
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Lane {
    Production,
    Staging,
    Research,
}

impl Lane {
    pub fn as_str(&self) -> &'static str {
        match self {
            Lane::Production => "PRODUCTION",
            Lane::Staging => "STAGING",
            Lane::Research => "RESEARCH",
        }
    }
    
    pub fn from_str(s: &str) -> Option<Self> {
        match s {
            "PRODUCTION" => Some(Lane::Production),
            "STAGING" => Some(Lane::Staging),
            "RESEARCH" => Some(Lane::Research),
            _ => None,
        }
    }
}

#[derive(Clone, Debug)]
pub struct EcosafetyState {
    pub risk_vector: RiskVector,
    pub lyapunov_v: LyapunovResidual,
    pub ker: KerTriad,
    pub timestamp_unix: i64,
}

impl EcosafetyState {
    pub fn new(
        risk_vector: RiskVector,
        ker: KerTriad,
        timestamp_unix: i64,
        weights: &[f64; 5],
    ) -> Self {
        let lyapunov_v = LyapunovResidual::from_risk_vector(&risk_vector, weights);
        Self {
            risk_vector,
            lyapunov_v,
            ker,
            timestamp_unix,
        }
    }
    
    pub fn validate_transition(
        &self,
        next: &EcosafetyState,
        weights: &[f64; 5],
        epsilon: f64,
    ) -> bool {
        let v_current = LyapunovResidual::from_risk_vector(&self.risk_vector, weights);
        let v_next = LyapunovResidual::from_risk_vector(&next.risk_vector, weights);
        safestep_ok(v_current, v_next, epsilon)
    }
    
    pub fn recompute_lyapunov(&mut self, weights: &[f64; 5]) {
        self.lyapunov_v = LyapunovResidual::from_risk_vector(&self.risk_vector, weights);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_corridor_normalization() {
        let corridor = CorridorBands::new(0.0, 0.5, 1.0).unwrap();
        
        assert_eq!(corridor.normalize(0.0).0, 0.0);
        assert!(corridor.normalize(0.25).0 < 0.5);
        assert_eq!(corridor.normalize(0.5).0, 0.5);
        assert!(corridor.normalize(0.75).0 > 0.5);
        assert_eq!(corridor.normalize(1.0).0, 1.0);
    }

    #[test]
    fn test_corridor_validation() {
        assert!(CorridorBands::new(0.0, 0.5, 1.0).is_some());
        assert!(CorridorBands::new(0.5, 0.0, 1.0).is_none());
        assert!(CorridorBands::new(0.0, 1.0, 0.5).is_none());
    }

    #[test]
    fn test_corridor_tightening() {
        let corridor = CorridorBands::new(0.0, 0.5, 1.0).unwrap();
        let tightened = corridor.tighten(0.1).unwrap();
        
        assert_eq!(tightened.hard, 0.9);
        assert_eq!(tightened.gold, 0.5);
        assert_eq!(tightened.safe, 0.0);
    }

    #[test]
    fn test_corridor_tightening_clamped() {
        let corridor = CorridorBands::new(0.0, 0.5, 1.0).unwrap();
        let tightened = corridor.tighten(0.5).unwrap();
        assert_eq!(tightened.hard, 0.85);
    }

    #[test]
    fn test_corridor_tightening_invalid() {
        let corridor = CorridorBands::new(0.0, 0.9, 1.0).unwrap();
        let result = corridor.tighten(0.15);
        assert!(result.is_none());
    }

    #[test]
    fn test_lyapunov_monotonicity() {
        let rv1 = RiskVector::new(
            RiskCoord(0.1),
            RiskCoord(0.1),
            RiskCoord(0.1),
            RiskCoord(0.1),
            RiskCoord(0.1),
        );
        
        let rv2 = RiskVector::new(
            RiskCoord(0.2),
            RiskCoord(0.2),
            RiskCoord(0.2),
            RiskCoord(0.2),
            RiskCoord(0.2),
        );
        
        let v1 = LyapunovResidual::from_risk_vector(&rv1, &DEFAULT_WEIGHTS);
        let v2 = LyapunovResidual::from_risk_vector(&rv2, &DEFAULT_WEIGHTS);
        
        assert!(v2.0 > v1.0);
        assert!(!safestep_ok(v2, v1, 0.0));
        assert!(safestep_ok(v1, v2, 1.0));
    }

    #[test]
    fn test_ker_lane_classification() {
        let deployable = KerTriad::new(0.95, 0.95, 0.10);
        assert_eq!(deployable.lane(), Lane::Production);
        assert!(deployable.is_deployable());
        
        let staging = KerTriad::new(0.87, 0.87, 0.15);
        assert_eq!(staging.lane(), Lane::Staging);
        assert!(staging.is_staging());
        
        let research = KerTriad::new(0.80, 0.80, 0.25);
        assert_eq!(research.lane(), Lane::Research);
        assert!(research.is_research());
    }

    #[test]
    fn test_risk_coord_bounds() {
        assert_eq!(RiskCoord::new(-0.5).0, 0.0);
        assert_eq!(RiskCoord::new(1.5).0, 1.0);
        assert_eq!(RiskCoord::new(0.5).0, 0.5);
    }

    #[test]
    fn test_max_risk() {
        let rv = RiskVector::new(
            RiskCoord(0.1),
            RiskCoord(0.3),
            RiskCoord(0.2),
            RiskCoord(0.15),
            RiskCoord(0.05),
        );
        
        assert_eq!(rv.max_risk().0, 0.3);
    }

    #[test]
    fn test_weighted_sum() {
        let rv = RiskVector::new(
            RiskCoord(0.1),
            RiskCoord(0.2),
            RiskCoord(0.3),
            RiskCoord(0.4),
            RiskCoord(0.5),
        );
        
        let result = rv.weighted_sum(&DEFAULT_WEIGHTS);
        assert!(result.0 > 0.2 && result.0 < 0.4);
    }

    #[test]
    fn test_eco_wealth() {
        let ker = KerTriad::new(0.95, 0.95, 0.10);
        let ew = ker.eco_wealth();
        assert!(ew > 0.8);
        
        let ker_bad = KerTriad::new(0.50, 0.50, 0.25);
        let ew_bad = ker_bad.eco_wealth();
        assert!(ew_bad < 0.2);
    }

    #[test]
    fn test_state_transition_validation() {
        let rv1 = RiskVector::new(
            RiskCoord(0.1),
            RiskCoord(0.1),
            RiskCoord(0.1),
            RiskCoord(0.1),
            RiskCoord(0.1),
        );
        let state1 = EcosafetyState::new(
            rv1,
            KerTriad::new(0.95, 0.95, 0.10),
            1000,
            &DEFAULT_WEIGHTS,
        );
        
        let rv2 = RiskVector::new(
            RiskCoord(0.09),
            RiskCoord(0.09),
            RiskCoord(0.09),
            RiskCoord(0.09),
            RiskCoord(0.09),
        );
        let state2 = EcosafetyState::new(
            rv2,
            KerTriad::new(0.95, 0.95, 0.08),
            1001,
            &DEFAULT_WEIGHTS,
        );
        
        assert!(state1.validate_transition(&state2, &DEFAULT_WEIGHTS, DEFAULT_EPSILON));
    }

    #[test]
    fn test_lane_string_conversion() {
        assert_eq!(Lane::Production.as_str(), "PRODUCTION");
        assert_eq!(Lane::from_str("STAGING"), Some(Lane::Staging));
        assert_eq!(Lane::from_str("INVALID"), None);
    }

    #[test]
    fn test_operational_score() {
        let corridor = CorridorBands::new(0.0, 0.5, 1.0).unwrap();
        let reference = CorridorBands::new(0.0, 0.5, 1.0).unwrap();
        assert_eq!(corridor.operational_score(&reference), 1.0);
        
        let tightened = corridor.tighten(0.1).unwrap();
        assert_eq!(tightened.operational_score(&reference), 0.9);
    }
}
