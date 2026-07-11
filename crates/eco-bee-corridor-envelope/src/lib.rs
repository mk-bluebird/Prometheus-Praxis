// eco_restoration_shard/crates/eco-bee-corridor-envelope/src/lib.rs

use serde::{Deserialize, Serialize};

/// BeeCorridorEnvelope v1: habitat continuity, activity, and exposure ceilings.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BeeCorridorEnvelope {
    // Identity and linkage
    pub corridor_id: String,         // e.g., "PHX-SUNFLOWER-BEE-01"
    pub region_id: String,           // district or Globe cell
    pub species_agent_id: String,    // links into Synthexis SpeciesAgent
    pub evidence_bundle_hex: String, // hex tag for BeeCorridorEvidenceBundle rowset
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
    pub emf_intensity_hive_height_vpm: f32, // corridor ceiling at hive height
    pub noise_level_db_a: f32,              // A-weighted dBA ceiling during active periods
    pub light_spill_lux: f32,               // max ALAN lux in bee corridors at night
    pub temperature_min_c: f32,             // lower bound of acceptable temperature range
    pub temperature_max_c: f32,             // upper bound of acceptable temperature range

    // Governance metadata
    pub roh_ceiling: f32, // risk-of-harm ceiling for bee plane (e.g. 0.30)
    pub ker_required: bool,
    pub lane: String, // "RESEARCH", "PILOT", "PROD"
}

/// BeeCorridorEvidenceBundle v1: thresholds and sources.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BeeCorridorEvidenceBundle {
    pub bundle_hex: String,
    pub species_agent_id: String,

    // Thresholds (safe maxima/minima)
    pub emf_safe_max_vpm: f32,
    pub noise_safe_max_db_a: f32,
    pub light_safe_max_lux: f32,
    pub temp_min_c: f32,
    pub temp_max_c: f32,

    pub floral_min_density_per_m2: f32,
    pub floral_connected_min_m2: f32,
    pub neonic_drift_max_ug_per_m2: f32,
    pub nesting_min_density_per_ha: f32,

    // Evidence source identifiers (DOI, hex tags, etc.)
    pub source_emf_1: String,
    pub source_emf_2: String,
    pub source_noise_1: String,
    pub source_noise_2: String,
    pub source_light_1: String,
    pub source_light_2: String,
    pub source_flora_1: String,
    pub source_flora_2: String,
    pub source_pesticide_1: String,
    pub source_pesticide_2: String,
    pub source_nesting_1: String,
    pub source_nesting_2: String,
}

/// SunflowerPolicyLedgerEntry captures a single policy step affecting bee corridors.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SunflowerPolicyLedgerEntry {
    pub node_id: String,
    pub corridor_id: String,
    pub policy_id: String,
    pub timestamp_utc_s: i64,
    pub eco_bee_impact_prev: f32,
    pub eco_bee_impact_new: f32,
    pub emf_intensity_hive_height_vpm: f32,
    pub noise_level_db_a: f32,
    pub light_spill_lux: f32,
}

/// Check that envelope ceilings do not exceed evidence-safe maxima.
pub fn envelope_respects_evidence(
    env: &BeeCorridorEnvelope,
    bundle: &BeeCorridorEvidenceBundle,
) -> bool {
    env.emf_intensity_hive_height_vpm <= bundle.emf_safe_max_vpm
        && env.noise_level_db_a <= bundle.noise_safe_max_db_a
        && env.light_spill_lux <= bundle.light_safe_max_lux
        && env.temperature_min_c >= bundle.temp_min_c
        && env.temperature_max_c <= bundle.temp_max_c
}

/// Check that habitat continuity meets minimum evidence thresholds.
pub fn envelope_meets_habitat_minima(
    env: &BeeCorridorEnvelope,
    bundle: &BeeCorridorEvidenceBundle,
) -> bool {
    env.floral_area_connected_m2 >= bundle.floral_connected_min_m2
        && env.nesting_site_density_per_ha >= bundle.nesting_min_density_per_ha
}

/// Check that a single ledger entry keeps EcoBeeImpactScore monotone.
pub fn eco_bee_impact_monotone(entry: &SunflowerPolicyLedgerEntry) -> bool {
    entry.eco_bee_impact_new >= entry.eco_bee_impact_prev
}

/// Check that a ledger entry does not exceed envelope exposure ceilings.
pub fn ledger_respects_envelope(
    env: &BeeCorridorEnvelope,
    entry: &SunflowerPolicyLedgerEntry,
) -> bool {
    entry.emf_intensity_hive_height_vpm <= env.emf_intensity_hive_height_vpm
        && entry.noise_level_db_a <= env.noise_level_db_a
        && entry.light_spill_lux <= env.light_spill_lux
}

#[cfg(feature = "kani-harnesses")]
mod kani_harnesses {
    use super::*;
    use kani::any;

    #[kani::proof]
    fn proof_envelope_respects_evidence() {
        let bundle: BeeCorridorEvidenceBundle = any();

        let env = BeeCorridorEnvelope {
            corridor_id: String::new(),
            region_id: String::new(),
            species_agent_id: bundle.species_agent_id.clone(),
            evidence_bundle_hex: bundle.bundle_hex.clone(),
            version: 1,
            floral_area_connected_m2: bundle.floral_connected_min_m2,
            nesting_site_density_per_ha: bundle.nesting_min_density_per_ha,
            pesticide_free_buffer_radius_m: 10.0,
            flight_density_per_min_m2: 1.0,
            foraging_success_rate: 0.8,
            shannon_diversity_index: 1.5,
            emf_intensity_hive_height_vpm: bundle.emf_safe_max_vpm,
            noise_level_db_a: bundle.noise_safe_max_db_a,
            light_spill_lux: bundle.light_safe_max_lux,
            temperature_min_c: bundle.temp_min_c,
            temperature_max_c: bundle.temp_max_c,
            roh_ceiling: 0.30,
            ker_required: true,
            lane: "PROD".to_string(),
        };

        assert!(envelope_respects_evidence(&env, &bundle));
        assert!(envelope_meets_habitat_minima(&env, &bundle));
    }

    #[kani::proof]
    fn proof_eco_bee_impact_monotone() {
        let prev: f32 = any();
        let new: f32 = any();

        let entry = SunflowerPolicyLedgerEntry {
            node_id: String::new(),
            corridor_id: String::new(),
            policy_id: String::new(),
            timestamp_utc_s: 0,
            eco_bee_impact_prev: prev,
            eco_bee_impact_new: new,
            emf_intensity_hive_height_vpm: 0.0,
            noise_level_db_a: 0.0,
            light_spill_lux: 0.0,
        };

        let accepted = eco_bee_impact_monotone(&entry);
        if accepted {
            assert!(entry.eco_bee_impact_new >= entry.eco_bee_impact_prev);
        }
    }

    #[kani::proof]
    fn proof_ledger_respects_envelope() {
        let env: BeeCorridorEnvelope = any();
        let entry: SunflowerPolicyLedgerEntry = any();

        let accepted =
            eco_bee_impact_monotone(&entry) && ledger_respects_envelope(&env, &entry);

        if accepted {
            assert!(entry.emf_intensity_hive_height_vpm <= env.emf_intensity_hive_height_vpm);
            assert!(entry.noise_level_db_a <= env.noise_level_db_a);
            assert!(entry.light_spill_lux <= env.light_spill_lux);
            assert!(entry.eco_bee_impact_new >= entry.eco_bee_impact_prev);
        }
    }
}
