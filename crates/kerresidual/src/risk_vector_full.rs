// 20. RiskVectorFull struct
// filename: kerresidual/src/risk_vector_full.rs
// destination: kerresidual/src/risk_vector_full.rs

use serde::{Deserialize, Serialize};

/// Full normalized risk vector including topology and sensor-trust planes.
/// All coordinates are in [0.0, 1.0].
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RiskVectorFull {
    /// Energy plane coordinates (examples).
    pub r_energy: f64,
    pub r_carbon: f64,
    /// Materials and hydrology.
    pub r_materials: f64,
    pub r_hydrology: f64,
    /// Biology / biodiversity planes.
    pub r_biology: f64,
    pub r_biodiversity: f64,
    /// Data-quality / sensor-trust coordinates.
    pub r_calib: f64,
    pub r_sigma: f64,
    /// Governance / topology risk coordinate.
    pub r_topology: f64,
    /// Optional aggregate trust coordinate if used as separate plane.
    pub r_trust: f64,
}

impl RiskVectorFull {
    /// Clamp all coordinates into [0.0, 1.0] to enforce boundedness.
    pub fn clamp01(&mut self) {
        self.r_energy = Self::clamp(self.r_energy);
        self.r_carbon = Self::clamp(self.r_carbon);
        self.r_materials = Self::clamp(self.r_materials);
        self.r_hydrology = Self::clamp(self.r_hydrology);
        self.r_biology = Self::clamp(self.r_biology);
        self.r_biodiversity = Self::clamp(self.r_biodiversity);
        self.r_calib = Self::clamp(self.r_calib);
        self.r_sigma = Self::clamp(self.r_sigma);
        self.r_topology = Self::clamp(self.r_topology);
        self.r_trust = Self::clamp(self.r_trust);
    }

    #[inline]
    fn clamp(x: f64) -> f64 {
        if x < 0.0 {
            0.0
        } else if x > 1.0 {
            1.0
        } else {
            x
        }
    }
}
