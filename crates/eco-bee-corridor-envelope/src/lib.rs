// eco_restoration_shard/crates/eco-bee-corridor-envelope/src/lib.rs

use serde::{Deserialize, Serialize};

/// BeeCorridorEnvelope v1: habitat continuity, activity, and exposure ceilings
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BeeCorridorEnvelope {
    // Identity and linkage
    pub corridor_id: String,              // e.g., "PHX-SUNFLOWER-BEE-01"
    pub region_id: String,                // district or Globe cell
    pub species_agent_id: String,         // links into Synthexis SpeciesAgent[file:92]
    pub evidence_bundle_hex: String,      // hex tag for BeeCorridorEvidenceBundle rowset
    pub version: u32,

    // Habitat continuity (structure/landscape)
    pub floral_area_connected_m2: f32,    // minimum connected floral area within corridor
    pub nesting_site_density_per_ha: f32, // nesting sites per hectare (wild + artificial)
    pub pesticide_free_buffer_radius_m: f32,

    // Bee activity indices (from telemetry or surveys)
    pub flight_density_per_min_m2: f32,   // normalized flight counts per minute per m²
    pub foraging_success_rate: f32,       // 0..1 fraction of foraging trips yielding nectar/pollen
    pub shannon_diversity_index: f32,     // H' for bee/guild biodiversity

    // Exposure ceilings (physical stressors)
    pub emf_intensity_hive_height_vpm: f32, // V/m or µT, corridor ceiling at hive height
    pub noise_level_db_a: f32,              // A-weighted dBA ceiling during active periods
    pub light_spill_lux: f32,               // max ALAN lux in bee corridors at night[file:92]
    pub temperature_min_c: f32,             // lower bound of acceptable temperature range
    pub temperature_max_c: f32,             // upper bound of acceptable temperature range

    // Governance metadata
    pub roh_ceiling: f32,                  // risk-of-harm ceiling for bee plane (e.g. 0.30)[file:68]
    pub ker_required: bool,               // must meet KER gates before deployment
    pub lane: String,                     // "RESEARCH", "PILOT", "PROD"
}
