// neuromorphic_band_guard.rs
// Non-actuating guardian for NeuromorphicMaterialProfile bands.
// #[forbid(unsafe_code)] enforced at crate root.

use serde::Deserialize;

#[derive(Debug, Clone, Deserialize)]
pub enum EnergyBand {
    UltraLow,
    Low,
    Medium,
    High,
}

#[derive(Debug, Clone, Deserialize)]
pub enum EnduranceBand {
    Low,
    Mid,
    High,
}

#[derive(Debug, Clone, Deserialize)]
pub enum VariabilityBand {
    Stable,
    Moderate,
    Unstable,
}

#[derive(Debug, Clone, Deserialize)]
pub enum EcoImpactBand {
    E0_0_25,
    E0_25_0_5,
    E0_5_0_75,
    E0_75_1_0,
}

#[derive(Debug, Clone, Deserialize)]
pub struct NeuromorphicMaterialProfile {
    pub profile_id: String,
    pub paper_id: String,
    pub device_label: String,
    pub material_system: String,
    pub energy_band: EnergyBand,
    pub endurance_band: EnduranceBand,
    pub variability_band: VariabilityBand,
    pub eco_impact_band: EcoImpactBand,
    pub is_compensated: bool,
    pub compensation_source_url: String,
    pub compensation_method: String,
    pub evidence_hex: String,
}

#[derive(Debug, Clone)]
pub struct CorridorPlan {
    pub vt_before: f64,
    pub vt_after: f64,
}
