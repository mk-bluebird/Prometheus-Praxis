use serde::{Deserialize, Serialize};
use time::OffsetDateTime;
use uuid::Uuid;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TopologyAudit {
    pub audit_id: Uuid,
    pub target_id: Uuid,
    pub itopology: f64,
    pub rtopology: f64,
    pub missing_manifests: Vec<String>,
    pub mislabelled_roles: Vec<String>,
    pub contract_violations: Vec<String>,
    pub drift_grace_seconds: i64,
    pub false_positive_probability: f64,
    pub timestamp: OffsetDateTime,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TopologyDriftEvent {
    pub audit: TopologyAudit,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProposedFix {
    pub fix_id: Uuid,
    pub target_id: Uuid,
    pub description: String,
    pub suggested_manifest_changes: Vec<String>,
    pub suggested_contract_enforcements: Vec<String>,
}
