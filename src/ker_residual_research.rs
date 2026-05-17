// filename: src/ker_residual_research.rs
// destination: eco_restoration_shard/src/ker_residual_research.rs

use serde::{Deserialize, Serialize};
use time::OffsetDateTime;
use uuid::Uuid;

use aln_core::Did;
use ecospine::{KER, RiskCoord};

/// 11. ResidualEngine calibration with real-world data

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CalibrationSample {
    pub sample_id: Uuid,
    pub region_id: String,
    pub timestamp: OffsetDateTime,
    pub ndvi: f64,                 // normalized difference vegetation index
    pub groundwater_level_m: f64,
    pub biodiversity_count: f64,   // species or index units
    pub residual_v: f64,           // current V_t
    pub ker: KER,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResidualEngineConfig {
    pub w_ndvi: f64,
    pub w_gw: f64,
    pub w_biodiversity: f64,
    pub target_ndvi: f64,
    pub target_gw_level_m: f64,
    pub target_biodiversity: f64,
}

pub struct ResidualEngineConfigCalibrator;

impl ResidualEngineConfigCalibrator {
    /// Calibrate residual weights so that higher NDVI, safer GW, and higher biodiversity
    /// push V_t down and K/E up in alignment with observed restoration.
    pub fn calibrate(samples: &[CalibrationSample]) -> ResidualEngineConfig {
        let w_ndvi = 0.33;
        let w_gw = 0.33;
        let w_biodiversity = 0.34;

        let target_ndvi = samples.iter().map(|s| s.ndvi).sum::<f64>() / (samples.len() as f64);
        let target_gw = samples
            .iter()
            .map(|s| s.groundwater_level_m)
            .sum::<f64>()
            / (samples.len() as f64);
        let target_biodiversity = samples
            .iter()
            .map(|s| s.biodiversity_count)
            .sum::<f64>()
            / (samples.len() as f64);

        ResidualEngineConfig {
            w_ndvi,
            w_gw,
            w_biodiversity,
            target_ndvi,
            target_gw_level_m: target_gw,
            target_biodiversity,
        }
    }

    pub fn to_risk_coords(
        cfg: &ResidualEngineConfig,
        sample: &CalibrationSample,
    ) -> (RiskCoord, RiskCoord, RiskCoord) {
        let r_ndvi = ((cfg.target_ndvi - sample.ndvi) / cfg.target_ndvi).max(0.0);
        let r_gw = ((cfg.target_gw_level_m - sample.groundwater_level_m)
            / cfg.target_gw_level_m)
            .max(0.0);
        let r_b = ((cfg.target_biodiversity - sample.biodiversity_count)
            / cfg.target_biodiversity)
            .max(0.0);

        (RiskCoord(r_ndvi), RiskCoord(r_gw), RiskCoord(r_b))
    }
}

/// 12. Multi-phase restoration residual

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum HydrologyPhase {
    Infiltration,
    Saturation,
    Flushing,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PhaseResidual {
    pub phase: HydrologyPhase,
    pub residual_v: f64,
    pub r_gw: RiskCoord,
    pub timestamp: OffsetDateTime,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MultiPhaseResidual {
    pub intervention_id: Uuid,
    pub basin_id: String,
    pub phases: Vec<PhaseResidual>,
}

impl MultiPhaseResidual {
    pub fn aggregate(&self) -> f64 {
        let n = self.phases.len() as f64;
        if n == 0.0 {
            return 0.0;
        }
        self.phases.iter().map(|p| p.residual_v).sum::<f64>() / n
    }
}

/// 13. Education multiplier anti-inflation guard

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EducationPromptEvent {
    pub event_id: Uuid,
    pub prompt_id: Uuid,
    pub steward_did: Did,
    pub completed_at: OffsetDateTime,
    pub reviewer_dids: Vec<Did>, // DID-signers from community review
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EducationGovernancePolicy {
    pub min_reviewer_signatures: u32,
    pub required_governance_did: Did,
}

impl EducationGovernancePolicy {
    pub fn is_valid_completion(&self, event: &EducationPromptEvent) -> bool {
        let unique_reviewers = event.reviewer_dids.len() as u32;
        unique_reviewers >= self.min_reviewer_signatures
    }
}

/// 14. KER band economics units

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerEconomicsUnits {
    pub k_unit: String, // dimensionless fraction of corridor-backed knowledge
    pub e_unit: String, // normalized ecosystem service gain per window
    pub r_unit: String, // dimensionless residual risk on [0,1]
}

impl KerEconomicsUnits {
    pub fn default_units() -> Self {
        KerEconomicsUnits {
            k_unit: "fraction_of_corridor_backed_knowledge".to_string(),
            e_unit: "normalized_ecosystem_service_gain".to_string(),
            r_unit: "normalized_residual_risk".to_string(),
        }
    }
}

/// 15. Residual slope b calculation with outlier handling

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResidualSample {
    pub t: f64,  // time in seconds since epoch
    pub v: f64,  // residual V_t
}

pub fn compute_b_slope(samples: &mut Vec<ResidualSample>, max_samples: usize) -> f64 {
    if samples.len() > max_samples {
        samples.sort_by(|a, b| a.t.partial_cmp(&b.t).unwrap());
        samples.drain(0..(samples.len() - max_samples));
    }

    if samples.len() < 2 {
        return 0.0;
    }

    let mean_v =
        samples.iter().map(|s| s.v).sum::<f64>() / (samples.len() as f64);
    let var_v =
        samples.iter().map(|s| (s.v - mean_v).powi(2)).sum::<f64>() / (samples.len() as f64);

    let std_v = var_v.sqrt();

    samples.retain(|s| (s.v - mean_v).abs() <= 2.0 * std_v);

    if samples.len() < 2 {
        return 0.0;
    }

    let n = samples.len() as f64;
    let mean_t =
        samples.iter().map(|s| s.t).sum::<f64>() / n;
    let mean_v =
        samples.iter().map(|s| s.v).sum::<f64>() / n;

    let mut num = 0.0;
    let mut den = 0.0;

    for s in samples.iter() {
        let dt = s.t - mean_t;
        let dv = s.v - mean_v;
        num += dt * dv;
        den += dt * dt;
    }

    if den == 0.0 {
        0.0
    } else {
        num / den
    }
}
