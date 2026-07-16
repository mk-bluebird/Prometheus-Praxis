// filename: crates/ai_node_shard/src/ai_datacenter_node.rs

use serde::{Deserialize, Serialize};
use steward_identity::StewardIdentity;

/// Rust mirror of AiDatacenterNode2026v1 telemetry and risk fields.
///
/// This struct focuses on raw telemetry and derived KER fields; normalization
/// into rx and vt is handled by kercore in a consistent way with the ALN particle.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct AiDatacenterNode2026v1 {
    // Identity and governance
    pub nodeid: String,
    pub region: String,
    pub lane: String,
    pub steward: StewardIdentity,
    pub twindow_start: String, // ISO-8601
    pub twindow_end: String,   // ISO-8601

    // Energy and efficiency
    pub core_energy_kwh_per_workload: f64,
    pub joules_per_inference: f64,
    pub pue: f64,

    // Carbon and eco-per-joule
    pub cue_kg_co2_per_kwh: f64,
    pub eco_per_joule: f64,

    // Bandwidth and utilization
    pub throughput_tokens_per_s: f64,
    pub throughput_inferences_per_s: f64,
    pub utilization_pct: f64,

    // Heat reuse and eco ratio
    pub ere: f64,
    pub eco_task_ratio_pct: f64,

    // Water and materials
    pub wue_l_per_kwh: f64,
    pub embodied_kg_co2eq: f64,

    // Topology
    pub topology_violation_count: i64,

    // Derived risk coordinates and residual
    pub r_pue: f64,
    pub r_cue: f64,
    pub r_eco_per_joule: f64,
    pub r_eco_task_ratio: f64,
    pub r_wue: f64,
    pub r_embodied: f64,
    pub r_topology: f64,
    pub r_energy: f64,
    pub r_joule_inf: f64,
    pub r_heat_reuse: f64,
    pub r_utilization: f64,
    pub r_bandwidth: f64,
    pub vt: f64,

    // KER and strength index
    pub k: f64,
    pub e: f64,
    pub r: f64,
    pub strength_index_s: f64,

    // Evidence / signing
    pub evidencehex: String,
    pub signinghex: String,
}

impl AiDatacenterNode2026v1 {
    /// Minimal sanity checks on raw telemetry and KER.
    pub fn validate(&self) -> Result<(), crate::AINodeError> {
        if self.core_energy_kwh_per_workload < 0.0 {
            return Err(crate::AINodeError::InvalidMeasurement(
                "core_energy_kwh_per_workload must be >= 0".into(),
            ));
        }
        if self.joules_per_inference < 0.0 {
            return Err(crate::AINodeError::InvalidMeasurement(
                "joules_per_inference must be >= 0".into(),
            ));
        }
        if self.pue < 0.0 {
            return Err(crate::AINodeError::InvalidMeasurement(
                "pue must be >= 0".into(),
            ));
        }
        if !(0.0..=100.0).contains(&self.utilization_pct) {
            return Err(crate::AINodeError::InvalidMeasurement(
                "utilization_pct must be in [0,100]".into(),
            ));
        }
        if !(0.0..=1.0).contains(&self.k)
            || !(0.0..=1.0).contains(&self.e)
            || !(0.0..=1.0).contains(&self.r)
        {
            return Err(crate::AINodeError::InvalidMeasurement(
                "KER factors must be in [0,1]".into(),
            ));
        }
        Ok(())
    }
}
