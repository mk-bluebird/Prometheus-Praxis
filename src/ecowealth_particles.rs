// filename: src/ecowealth_particles.rs

use aln_core::{Did, HexHash};
use ecospine::{
    KER, Residual, RiskCoord, PlaneId, LaneId, CorridorBands,
    InvariantError, check_residual_nonincrease,
};

/// Regional eco-ledger particle: one small, verifiable action.
#[derive(Debug, Clone)]
pub struct EcoLedgerParticleRegion {
    pub region_id: String,
    pub region_lane: LaneId,
    pub actor_did: Did,
    pub bostrom_addr: String,
    pub action_code: String, // e.g. "INVASIVE_REMOVAL", "IRRIGATION_REPAIR"
    pub ker: KER,
    pub residual_before: Residual,
    pub residual_after: Residual,
    pub corridor_bands: Vec<CorridorBands>,
    pub evidence_hex: HexHash,
}

/// Hydrology / groundwater constraint equation object.
#[derive(Debug, Clone)]
pub struct HydrologyConstraint {
    pub basin_id: String,
    pub equation_id: String,
    pub equation_hex: HexHash,
    pub gwrisk_max: f32,       // normalized r_GW <= 1
    pub recharge_min_m3: f32,
    pub withdrawal_max_m3: f32,
    pub nonoffsettable: bool,  // hydrology plane is non-offsettable
}

/// Ecological sensor telemetry particle.
#[derive(Debug, Clone)]
pub struct EcoSensorTelemetry {
    pub sensor_id: String,
    pub asset_id: String,
    pub plane: PlaneId,
    pub metric_type: String,   // "SOIL_MOISTURE", "FLOW", "TEMP", "CANOPY"
    pub value: f32,
    pub unit: String,
    pub timestamp_iso: String,
    pub risk_coord: RiskCoord, // r_j in [0,1] derived from value and corridor
}

/// Urban zoning / land-use shard.
#[derive(Debug, Clone)]
pub struct ZoningShard {
    pub jurisdiction: String,
    pub parcel_id: String,
    pub zone_code: String,
    pub setback_m: f32,
    pub max_height_m: f32,
    pub flood_overlay: bool,
    pub tree_corridor_allowed: bool,
    pub wetland_allowed: bool,
    pub topology_risk: RiskCoord, // r_topology
}

/// Non-financial eco-wealth portfolio view.
#[derive(Debug, Clone)]
pub struct EcoWealthPortfolio {
    pub region_id: String,
    pub identity_did: Did,
    pub tree_biomass_t: f32,
    pub pollinator_index: f32,
    pub shade_canopy_m2: f32,
    pub thermal_index: f32,
    pub biodiversity_index: f32,
    pub ker: KER,
}

/// Stewardship / governance role shard.
#[derive(Debug, Clone)]
pub struct StewardRoleShard {
    pub role_id: String,   // "BLOCK_STEWARD", "WATERSHED_COUNCIL", ...
    pub steward_did: Did,
    pub jurisdiction: String,
    pub responsibilities: Vec<String>,
    pub lane_profile: LaneId,
}

/// Education / apprenticeship prompt shard.
#[derive(Debug, Clone)]
pub struct EducationPromptShard {
    pub prompt_id: String,
    pub topic: String,
    pub difficulty_band: String, // "INTRO", "INTERMEDIATE", "ADVANCED"
    pub docspec_ref: String,
    pub ker: KER,
}

/// Ecological cost and co-benefit pricing shard.
#[derive(Debug, Clone)]
pub struct EcoCostBenefitShard {
    pub intervention_type: String, // "TREE_PLANTING", "COOL_ROOF", ...
    pub co2_avoided_t: f32,
    pub cooling_deg_c: f32,
    pub biodiversity_delta: f32,
    pub capex_usd: f32,
    pub opex_usd: f32,
    pub ecoper_cost: f32,         // E per unit cost
}

/// Disaster / extreme-event scenario shard.
#[derive(Debug, Clone)]
pub struct DisasterScenarioShard {
    pub scenario_id: String,
    pub event_type: String, // "HEATWAVE", "DROUGHT", "FLOOD", "WILDFIRE"
    pub region_id: String,
    pub return_period_years: f32,
    pub linked_node_ids: Vec<String>,
    pub emergency_protocol_refs: Vec<String>,
}

/// Cross-constellation interoperability index shard.
#[derive(Debug, Clone)]
pub struct InteropIndexShard {
    pub ecosystem_id: String, // "CARBON_MARKET_X", "CITY_DATA_PLATFORM_Y"
    pub api_id: String,
    pub ker_band: KER,
    pub nonoffsettable_flags: Vec<String>,
    pub governance_profile: String,
}

/// Ecoper-joule asset plane.
#[derive(Debug, Clone)]
pub struct EcoperJouleAsset {
    pub workload_id: String,
    pub node_id: String,
    pub karmadelta: f32,
    pub energy_joules: f32,
    pub ecoper_joule: f32, // karmadelta / energy_joules normalized
}

/// Restoration radius / MAR asset.
#[derive(Debug, Clone)]
pub struct RestorationRadiusAsset {
    pub radius_id: String,
    pub region_id: String,
    pub pollutant_mass_removed_kg: f32,
    pub karmadelta: f32,
    pub rgw: RiskCoord,
    pub mar_volume_m3: f32,
}

/// Plane weights and non-offsettable bands.
#[derive(Debug, Clone)]
pub struct PlaneWeightsRow {
    pub plane: PlaneId,
    pub weight: f32,
    pub nonoffsettable: bool,
    pub corridor_bounds: (f32, f32),
}

/// Blastradius / neighbouring-zone asset.
#[derive(Debug, Clone)]
pub struct BlastRadiusAsset {
    pub shard_id: String,
    pub scope: String,
    pub region_id: String,
    pub radii_m: Vec<f32>,
    pub ker_band: KER,
    pub continuity_grade: String,
    pub sovereignty_tags: Vec<String>,
    pub hex_descriptor: HexHash,
}

/// Lane governance and KER trajectory asset.
#[derive(Debug, Clone)]
pub struct LaneStatusShard {
    pub lane_id: LaneId,
    pub ker_k_agg: f32,
    pub ker_e_agg: f32,
    pub ker_r_trend: f32,
    pub admissibility_predicate: String,
}

/// Topology risk / alignment asset.
#[derive(Debug, Clone)]
pub struct TopologyRiskAsset {
    pub itopology: f32,
    pub rtopology: RiskCoord,
    pub missing_manifests: u32,
    pub mislabelled_roles: u32,
    pub lane_violations: u32,
}

/// Large-particle file summary.
#[derive(Debug, Clone)]
pub struct LargeParticleFile {
    pub file_hash: HexHash,
    pub size_bytes: u64,
    pub chunk_hint: u32,
    pub summary_level: String, // "HEADERS_ONLY", "BLOCK_STATS"
}

/// Large-particle block registry entry.
#[derive(Debug, Clone)]
pub struct LargeParticleBlock {
    pub file_hash: HexHash,
    pub block_index: u32,
    pub offset_bytes: u64,
    pub length_bytes: u32,
    pub aggregate_hex: HexHash,
}

/// Healthcare RoH and detox corridor asset.
#[derive(Debug, Clone)]
pub struct HealthcareDetoxAsset {
    pub roh_kernel_id: String,
    pub treatment_course_id: String,
    pub roh_coords: Vec<RiskCoord>,
    pub detox_corridor_state: String,
    pub hardware_tag: String, // e.g. "MT6883"
    pub qpu_catalog_ref: String,
}

/// QPU catalog / virtual hardware asset.
#[derive(Debug, Clone)]
pub struct QPUCatalogAsset {
    pub virtual_node_id: String,
    pub energy_domain: String,
    pub cost_reduction_score: f32,
    pub continuity_contract_id: String,
    pub nonactuating: bool,
}

/// KnowledgeEcoScore and reward ledger asset.
#[derive(Debug, Clone)]
pub struct KnowledgeEcoScoreShard {
    pub repo_id: String,
    pub schema_id: String,
    pub shard_id: String,
    pub ker: KER,
    pub rationale: String,
    pub signing_did: Did,
    pub reward_eligible: bool,
}

/// Invariant: regional eco-ledger particle must not increase residual V outside interior.
pub fn check_ecoledger_invariant(p: &EcoLedgerParticleRegion) -> Result<(), InvariantError> {
    check_residual_nonincrease(&p.residual_before, &p.residual_after)
}
