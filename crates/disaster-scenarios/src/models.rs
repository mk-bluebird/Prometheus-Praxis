use serde::{Deserialize, Serialize};
use uuid::Uuid;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Protocol {
    pub steps: Vec<String>,
    pub contact_dids: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DisasterScenario {
    pub scenario_id: Uuid,
    pub disaster_type: String, // "heatwave","drought","flood","wildfire"
    pub region_id: String,
    pub severity: f64,         // 0..1
    pub description: String,
    pub linked_nodes: Vec<Uuid>,
    pub emergency_protocol: Protocol,
    pub resilience_interventions: Vec<String>,
    pub probability: f64,
    pub cluster_id: Option<Uuid>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScenarioAlert {
    pub scenario_id: Uuid,
    pub activated_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResiliencePlan {
    pub plan_id: Uuid,
    pub scenario_id: Uuid,
    pub linked_nodes: Vec<Uuid>,
    pub interventions: Vec<String>,
}
