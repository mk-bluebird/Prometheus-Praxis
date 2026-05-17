// filename: src/ker_nanoswarm_and_atmospheric.rs
// destination: eco_restoration_shard/src/ker_nanoswarm_and_atmospheric.rs

use serde::{Deserialize, Serialize};
use time::OffsetDateTime;
use uuid::Uuid;

use aln_core::{Did, HexHash};
use ecospine::{KER, RiskCoord};

/// 16. Nanoswarm KER contribution with Gaussian blastradius

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NanoswarmKernel {
    pub center_node_id: Uuid,
    pub sigma_hops: f64,        // Gaussian width in graph hops
    pub sigma_km: f64,          // optional physical radius
    pub peak_k_delta: f64,      // K gain at center
    pub peak_e_delta: f64,      // E gain at center
    pub peak_r_delta: f64,      // R change at center (usually negative)
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NanoswarmEffect {
    pub node_id: Uuid,
    pub graph_distance_hops: f64,
    pub geo_distance_km: f64,
    pub k_delta: f64,
    pub e_delta: f64,
    pub r_delta: f64,
}

impl NanoswarmKernel {
    /// Compute deltas as a Gaussian of graph distance; sigma is linked to blastradius hops.
    pub fn project_to_node(&self, node_id: Uuid, distance_hops: f64, distance_km: f64) -> NanoswarmEffect {
        let w = (-0.5 * (distance_hops / self.sigma_hops).powi(2)).exp();
        NanoswarmEffect {
            node_id,
            graph_distance_hops: distance_hops,
            geo_distance_km: distance_km,
            k_delta: self.peak_k_delta * w,
            e_delta: self.peak_e_delta * w,
            r_delta: self.peak_r_delta * w,
        }
    }
}

/// Extension hook for T10 eco-pricing ranker:
/// impact_cost_ratio can be adjusted to account for nanoswarm dispersion.

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NanoswarmPricingAdjustment {
    pub intervention_id: String,
    pub kernel: NanoswarmKernel,
    pub blastradius_hex: HexHash,
}

/// 17. Shadow residual engine for monoculture risk mitigation

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResidualSnapshot {
    pub snapshot_id: Uuid,
    pub lane_id: Uuid,
    pub region_id: String,
    pub ker: KER,
    pub v_primary: f64,
    pub v_shadow: f64,
    pub epsilon_max: f64,
}

pub trait ShadowResidualEngine {
    fn compute_v(&self, ker: &KER) -> f64;
}

impl ResidualSnapshot {
    /// Enforce |v_primary - v_shadow| <= epsilon_max before promotions.
    pub fn is_consistent(&self) -> bool {
        (self.v_primary - self.v_shadow).abs() <= self.epsilon_max
    }
}

/// 18. KerSource for sensor/model provenance

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum KerSource {
    RawTelemetry,
    ConsensusCorrected,
    GovernanceOverride,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerWithSource {
    pub ker: KER,
    pub source: KerSource,
    pub evidence_hash: HexHash,
}

/// 19. EcoperJouleRecord fields for T02 router

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoperJouleRecord {
    pub record_id: Uuid,
    pub workload_id: String,
    pub node_id: Uuid,
    pub nonactuating_contract_id: Option<Uuid>,
    pub timestamp: OffsetDateTime,
    pub karmadelta: f64,
    pub energy_joules: f64,
    pub ecoper_joule: f64,
    pub current_workload_joules: f64,
    pub eco_cost_per_joule: f64,
    pub ker_residual_penalty_per_joule: f64,
    pub ker_snapshot: KER,
    pub tags: Vec<String>,
}

/// 20. AtmosphericCorridor for laser-based restoration

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AtmosphericCorridor {
    pub corridor_id: Uuid,
    pub global_radius_km: f64,
    pub altitude_band_km: (f64, f64),
    pub ker_projection: KER,
    pub blastradius_overlap_hex: HexHash,
    pub created_at: OffsetDateTime,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AtmosphericIntervention {
    pub intervention_id: Uuid,
    pub description: String,
    pub corridor: AtmosphericCorridor,
    pub proposer_did: Did,
}
