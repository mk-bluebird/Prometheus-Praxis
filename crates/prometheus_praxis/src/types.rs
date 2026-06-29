// filename: crates/prometheus_praxis/src/types.rs
// destination: mk-bluebird/eco_restoration_shard
// edition: 2024, rust-version = "1.85"

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};
use thiserror::Error;
use bioscale_metrics::Bounded01;
use transhuman_rights_core::{EqualityEnvelope, NeuroRightsEnvelope};

/// Category of the execution task.
/// Matches governance and ecosafety corridors used across EcoNet / EcoFort.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum TaskKind {
    EcoRestoration,
    SmartCityUpgrade,
    HealthcareProcedure,
    AugmentationUpgrade,
    PaymentProgramRollout,
}

/// High-level task definition ingested from strategic layers.
/// This struct is the primary, DID-addressable task envelope.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PrometheusTask {
    /// Globally unique identifier for the task (e.g. UUID, hex-stamped ID).
    pub task_id: String,

    /// Category of work; drives corridor selection and ecosafety checks.
    pub kind: TaskKind,

    /// Jurisdictional reference (e.g. "US-AZ-Phoenix").
    /// Used to bind tasks into region-specific corridors and DefinitionRegistry entries.
    pub jurisdiction_ref: String,

    /// Service class, e.g. "ServiceClassBasic", "ServiceClassCritical".
    /// This allows schedulers and guards to prioritize and route tasks.
    pub service_class: String,

    /// Eco-impact target in [0.0, 1.0] (Bounded01 from bioscale_metrics).
    /// Typically aligned with KER ecoimpact E windows for the corridor.
    pub eco_target: Bounded01,

    /// Risk-of-harm ceiling in [0.0, 1.0] (Bounded01 from bioscale_metrics).
    /// Lower values encode stricter risk corridors for the task.
    pub roh_target: Bounded01,

    /// Neurorights envelope for any augmented citizens affected by the task.
    /// This is a typed contract from transhuman_rights_core.
    pub neurorights_env: NeuroRightsEnvelope,

    /// Equality and soulsafety envelope, ensuring non-discrimination and equity.
    pub equality_env: EqualityEnvelope,
}

/// A single step within an execution plan.
/// Each step carries its own bounded scalars so guards can reason locally.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlanStep {
    /// Unique step identifier within the plan.
    pub step_id: String,

    /// Human-readable description of the step.
    pub description: String,

    /// Estimated risk-of-harm contribution for this step, in [0.0, 1.0].
    pub estimated_roh: Bounded01,

    /// Estimated eco delta for this step, in [0.0, 1.0].
    /// Positive values represent eco-positive actions; near-zero for neutral.
    pub estimated_eco_delta: Bounded01,
}

/// Complete execution plan generated for a task.
/// Guards, schedulers, and CI can consume this struct directly.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExecutionPlan {
    /// Source Prometheus task this plan realizes.
    pub task: PrometheusTask,

    /// Ordered list of execution steps.
    pub steps: Vec<PlanStep>,
}

/// Local error type for Prometheus-Praxis bounded scalar and plan violations.
/// This keeps wiring simple and avoids leaking internal error enums to callers.
#[derive(Debug, Error)]
pub enum PrometheusError {
    #[error("Bounded01 value {0} is out of the required range [0.0, 1.0]")]
    OutOfRange(f64),

    #[error("Execution plan has no steps defined")]
    EmptyPlan,

    #[error("Jurisdiction reference is empty")]
    EmptyJurisdiction,

    #[error("Service class is empty")]
    EmptyServiceClass,
}

impl PrometheusTask {
    /// Lightweight constructor that validates jurisdiction and service_class
    /// before returning a task envelope.
    pub fn new(
        task_id: impl Into<String>,
        kind: TaskKind,
        jurisdiction_ref: impl Into<String>,
        service_class: impl Into<String>,
        eco_target: Bounded01,
        roh_target: Bounded01,
        neurorights_env: NeuroRightsEnvelope,
        equality_env: EqualityEnvelope,
    ) -> Result<Self, PrometheusError> {
        let task_id = task_id.into();
        let jurisdiction_ref = jurisdiction_ref.into();
        let service_class = service_class.into();

        if jurisdiction_ref.trim().is_empty() {
            return Err(PrometheusError::EmptyJurisdiction);
        }

        if service_class.trim().is_empty() {
            return Err(PrometheusError::EmptyServiceClass);
        }

        Ok(Self {
            task_id,
            kind,
            jurisdiction_ref,
            service_class,
            eco_target,
            roh_target,
            neurorights_env,
            equality_env,
        })
    }
}

impl ExecutionPlan {
    /// Simple guard ensuring that a plan contains at least one step.
    pub fn validate(&self) -> Result<(), PrometheusError> {
        if self.steps.is_empty() {
            return Err(PrometheusError::EmptyPlan);
        }
        Ok(())
    }
}
