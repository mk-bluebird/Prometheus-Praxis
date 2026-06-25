use crate::types::{PrometheusTask, TaskKind};
use crate::guards::PrometheusGuards;

#[derive(Debug)]
pub struct ExecutionPlan {
    pub task: PrometheusTask,
    pub steps: Vec<PlanStep>,
}

#[derive(Debug)]
pub struct PlanStep {
    pub step_id: String,
    pub description: String,
    pub estimated_roh: f32,
    pub estimated_eco_delta: f32,
}

pub trait PrometheusPlanner {
    fn plan(&self, task: PrometheusTask) -> anyhow::Result<ExecutionPlan>;
}

pub struct DefaultPrometheusPlanner<G: PrometheusGuards> {
    guards: G,
}

impl<G: PrometheusGuards> DefaultPrometheusPlanner<G> {
    pub fn new(guards: G) -> Self {
        Self { guards }
    }
}

impl<G: PrometheusGuards> PrometheusPlanner for DefaultPrometheusPlanner<G> {
    fn plan(&self, task: PrometheusTask) -> anyhow::Result<ExecutionPlan> {
        // High-level: delegate to domain-specific planners,
        // then run guard checks before returning. [file:1][file:14]
        self.guards.pre_plan_check(&task)?;
        let steps = match task.kind {
            TaskKind::EcoRestoration => eco_restoration_plan(&task),
            TaskKind::SmartCityUpgrade => smart_city_plan(&task),
            TaskKind::HealthcareProcedure => healthcare_plan(&task),
            TaskKind::AugmentationUpgrade => augmentation_plan(&task),
            TaskKind::PaymentProgramRollout => payment_program_plan(&task),
        }?;
        self.guards.post_plan_check(&task, &steps)?;
        Ok(ExecutionPlan { task, steps })
    }
}

// Domain-specific planners (pure functions – no side effects)
fn eco_restoration_plan(task: &PrometheusTask) -> anyhow::Result<Vec<PlanStep>> {
    // Here you would integrate Tellus-Synoptikos, Nova Terra, Gaia-Orbis, etc. [file:1]
    // No torque/current/stim fields should appear in these structures.
    Ok(vec![
        PlanStep {
            step_id: format!("{}-soil-assessment", task.task_id),
            description: "Run Boden-Wesen soil assessment across target polygon.",
            estimated_roh: 0.05,
            estimated_eco_delta: 0.10,
        },
    ])
}

// ... similarly for smart_city_plan, healthcare_plan, augmentation_plan, payment_program_plan
