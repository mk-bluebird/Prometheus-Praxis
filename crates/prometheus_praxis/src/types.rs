use serde::{Deserialize, Serialize};
use bioscale_metrics::Bounded01;
use transhuman_rights_core::{NeuroRightsEnvelope, EqualityEnvelope}; // from your existing crate [file:14]

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum TaskKind {
    EcoRestoration,
    SmartCityUpgrade,
    HealthcareProcedure,
    AugmentationUpgrade,
    PaymentProgramRollout,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PrometheusTask {
    pub task_id: String,
    pub kind: TaskKind,
    pub jurisdiction_ref: String,
    pub service_class: String, // e.g. "ServiceClassBasic"
    pub eco_target: Bounded01,
    pub roh_target: Bounded01,
    pub neurorights_env: NeuroRightsEnvelope,
    pub equality_env: EqualityEnvelope,
}
