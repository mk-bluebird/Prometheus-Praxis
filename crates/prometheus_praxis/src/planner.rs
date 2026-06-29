// aletheion/prometheus/src/planner/prometheus_planner.rs

//! Planning logic for translating Prometheus tasks into execution steps.
//! This module is strictly non-actuating and pure.

#![forbid(unsafe_code)]

use thiserror::Error;

use crate::guards::PrometheusGuards;
use crate::types::{Bounded01, ExecutionPlan, PlanStep, PrometheusTask, TaskKind};

/// Errors that can occur during planning.
#[derive(Debug, Error)]
pub enum PlannerError {
    #[error("Guard check failed: {0}")]
    GuardFailed(String),
    #[error("Unsupported task kind: {0:?}")]
    UnsupportedTask(TaskKind),
}

/// Trait for domain-specific planners.
pub trait PrometheusPlanner {
    /// Generates an execution plan for the given task.
    fn plan(&self, task: PrometheusTask) -> Result<ExecutionPlan, PlannerError>;
}

/// Default planner implementation that delegates to guards and domain logic.
pub struct DefaultPrometheusPlanner<G: PrometheusGuards> {
    guards: G,
}

impl<G: PrometheusGuards> DefaultPrometheusPlanner<G> {
    /// Constructs a new planner with the given guard implementation.
    pub fn new(guards: G) -> Self {
        Self { guards }
    }
}

impl<G: PrometheusGuards> PrometheusPlanner for DefaultPrometheusPlanner<G> {
    fn plan(&self, task: PrometheusTask) -> Result<ExecutionPlan, PlannerError> {
        // Pre-plan safety and governance checks.
        self.guards
            .pre_plan_check(&task)
            .map_err(|e| PlannerError::GuardFailed(e.to_string()))?;

        // Delegate to domain-specific pure planners.
        let steps = match task.kind {
            TaskKind::EcoRestoration => plan_eco_restoration(&task),
            TaskKind::SmartCityUpgrade => plan_smart_city(&task),
            TaskKind::HealthcareProcedure => plan_healthcare(&task),
            TaskKind::AugmentationUpgrade => plan_augmentation(&task),
            TaskKind::PaymentProgramRollout => plan_payment(&task),
        }
        .map_err(PlannerError::UnsupportedTask)?;

        // Post-plan safety and governance checks.
        self.guards
            .post_plan_check(&task, &steps)
            .map_err(|e| PlannerError::GuardFailed(e.to_string()))?;

        Ok(ExecutionPlan { task, steps })
    }
}

// ---------- Domain-specific plan functions (pure, non-actuating) ----------

fn plan_eco_restoration(task: &PrometheusTask) -> Result<Vec<PlanStep>, TaskKind> {
    if task.kind != TaskKind::EcoRestoration {
        return Err(task.kind);
    }

    Ok(vec![PlanStep {
        step_id: format!("{}-soil-assessment", task.task_id),
        description: "Run Boden-Wesen soil assessment across target polygon.".to_string(),
        estimated_roh: Bounded01::new(0.05)
            .expect("bounded RoH value must be within [0,1]"),
        estimated_eco_delta: Bounded01::new(0.10)
            .expect("bounded eco delta value must be within [0,1]"),
    }])
}

fn plan_smart_city(task: &PrometheusTask) -> Result<Vec<PlanStep>, TaskKind> {
    if task.kind != TaskKind::SmartCityUpgrade {
        return Err(task.kind);
    }

    Ok(vec![PlanStep {
        step_id: format!("{}-mesh-sync", task.task_id),
        description: "Synchronize Heim-Netz mesh nodes for district upgrade.".to_string(),
        estimated_roh: Bounded01::new(0.02)
            .expect("bounded RoH value must be within [0,1]"),
        estimated_eco_delta: Bounded01::new(0.05)
            .expect("bounded eco delta value must be within [0,1]"),
    }])
}

fn plan_healthcare(task: &PrometheusTask) -> Result<Vec<PlanStep>, TaskKind> {
    if task.kind != TaskKind::HealthcareProcedure {
        return Err(task.kind);
    }

    Ok(vec![PlanStep {
        step_id: format!("{}-nanoswarm-calibration", task.task_id),
        description:
            "Calibrate nanoswarm therapeutic envelope under Hestia-Continuus.".to_string(),
        estimated_roh: Bounded01::new(0.15)
            .expect("bounded RoH value must be within [0,1]"),
        estimated_eco_delta: Bounded01::new(0.00)
            .expect("bounded eco delta value must be within [0,1]"),
    }])
}

fn plan_augmentation(task: &PrometheusTask) -> Result<Vec<PlanStep>, TaskKind> {
    if task.kind != TaskKind::AugmentationUpgrade {
        return Err(task.kind);
    }

    Ok(vec![PlanStep {
        step_id: format!("{}-ota-forward", task.task_id),
        description: "Execute forward-only OTA via Perkunos-Nexus.".to_string(),
        estimated_roh: Bounded01::new(0.10)
            .expect("bounded RoH value must be within [0,1]"),
        estimated_eco_delta: Bounded01::new(0.00)
            .expect("bounded eco delta value must be within [0,1]"),
    }])
}

fn plan_payment(task: &PrometheusTask) -> Result<Vec<PlanStep>, TaskKind> {
    if task.kind != TaskKind::PaymentProgramRollout {
        return Err(task.kind);
    }

    Ok(vec![PlanStep {
        step_id: format!("{}-biopay-activate", task.task_id),
        description: "Activate BioPay MOP corridor for district.".to_string(),
        estimated_roh: Bounded01::new(0.01)
            .expect("bounded RoH value must be within [0,1]"),
        estimated_eco_delta: Bounded01::new(0.02)
            .expect("bounded eco delta value must be within [0,1]"),
    }])
}
