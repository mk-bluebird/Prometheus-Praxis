// Filename: crates/cyboquationmark_workload/src/lib.rs
use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub enum MediaClass {
    Water,
    Air,
    Mixed,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub enum HydraulicImpact {
    Neutral,
    LocalPerturbation,
    GlobalPerturbation,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub enum WorkloadKind {
    ValveMove,
    Analytics,
    NanoswarmActuation,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ValveMovePayload {
    pub valve_id: String,
    pub open_fraction: f64, // 0.0–1.0
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnalyticsPayload {
    pub job_id: String,
    pub input_shard_id: String,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct NanoswarmPayload {
    pub swarm_id: String,
    pub target_node_id: String,
    pub duration_s: f64,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum CyboquationmarkPayload {
    ValveMove(ValveMovePayload),
    Analytics(AnalyticsPayload),
    NanoswarmActuation(NanoswarmPayload),
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CyboquationmarkVariant {
    pub kind: WorkloadKind,
    pub payload: CyboquationmarkPayload,
    pub media_class: MediaClass,
    pub hydraulic_impact: HydraulicImpact,
    pub safety_factor: f64,
    pub energy_req_j: f64,
    pub expected_delta_vt: f64,
}

pub trait CyboquaticWorkload {
    fn energyreq_j(&self) -> f64;
    fn safetyfactor(&self) -> f64;
    fn media_class(&self) -> MediaClass;
    fn hydraulicimpact(&self) -> HydraulicImpact;
    fn expected_delta_vt(&self) -> f64;
}

impl CyboquaticWorkload for CyboquationmarkVariant {
    fn energyreq_j(&self) -> f64 {
        self.energy_req_j
    }

    fn safetyfactor(&self) -> f64 {
        self.safety_factor
    }

    fn media_class(&self) -> MediaClass {
        self.media_class
    }

    fn hydraulicimpact(&self) -> HydraulicImpact {
        self.hydraulic_impact
    }

    fn expected_delta_vt(&self) -> f64 {
        self.expected_delta_vt
    }
}

// Example JSON interchange helper
pub fn to_json(w: &CyboquationmarkVariant) -> serde_json::Result<String> {
    serde_json::to_string(w)
}

pub fn from_json(s: &str) -> serde_json::Result<CyboquationmarkVariant> {
    serde_json::from_str(s)
}
