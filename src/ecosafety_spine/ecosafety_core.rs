#![no_std]
use core::cmp::Ordering;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct RiskCoord(pub f64);

impl RiskCoord {
    pub fn new(value: f64) -> Self {
        RiskCoord(value.clamp(0.0, 1.0))
    }
    
    pub fn is_safe(self) -> bool {
        self.0 <= 0.13
    }
    
    pub fn is_gold(self) -> bool {
        self.0 <= 0.10
    }
}

#[derive(Clone, Copy, Debug)]
pub struct CorridorBands {
    pub safe: f64,
    pub gold: f64,
    pub hard: f64,
}

impl CorridorBands {
    pub fn normalize(&self, raw_value: f64) -> RiskCoord {
        if raw_value <= self.safe {
            RiskCoord(0.0)
        } else if raw_value >= self.hard {
            RiskCoord(1.0)
        } else if raw_value <= self.gold {
            let t = (raw_value - self.safe) / (self.gold - self.safe);
            RiskCoord(0.5 * t)
        } else {
            let t = (raw_value - self.gold) / (self.hard - self.gold);
            RiskCoord(0.5 + 0.5 * t)
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
    pub fn max_risk(&self) -> RiskCoord {
        [
            self.r_energy,
            self.r_hydraulic,
            self.r_biology,
            self.r_carbon,
            self.r_materials,
        ]
        .iter()
        .max_by(|a, b| a.0.partial_cmp(&b.0).unwrap_or(Ordering::Equal))
        .copied()
        .unwrap()
    }
}

#[derive(Clone, Copy, Debug)]
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
}

pub fn safestep_ok(v_prev: LyapunovResidual, v_next: LyapunovResidual, epsilon: f64) -> bool {
    v_next.0 <= v_prev.0 + epsilon
}

#[derive(Clone, Debug)]
pub struct KerTriad {
    pub k: f64,
    pub e: f64,
    pub r: f64,
}

impl KerTriad {
    pub fn is_deployable(&self) -> bool {
        self.k >= 0.90 && self.e >= 0.90 && self.r <= 0.13
    }
}
