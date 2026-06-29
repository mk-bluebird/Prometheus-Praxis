// Path: src/governance/guards/prometheus_guards.rs

#![forbid(unsafe_code)]

use thiserror::Error;

use crate::planner::PlanStep;
use crate::types::PrometheusTask;

use transhuman_rights_core::DefaultNeurorightsGuard;
use equality_payment_guard::EqualityPaymentGuard;
use aug_fingerprint_guard::AugFingerprintGuard;
use bioscale_metrics::Bounded01;

/// Errors originating from governance guard violations.
#[derive(Debug, Error)]
pub enum GuardError {
    #[error("Neurorights violation: {0}")]
    NeuroRights(String),
    #[error("Equality violation: {0}")]
    Equality(String),
    #[error("ROH ceiling exceeded: {0} > 0.30")]
    RoHExceeded(f64),
}

/// Trait for composable governance guards on Prometheus tasks.
pub trait PrometheusGuards {
    /// Validates the task before planning begins.
    fn pre_plan_check(&self, task: &PrometheusTask) -> Result<(), GuardError>;

    /// Validates the generated steps before execution is authorized.
    fn post_plan_check(&self, task: &PrometheusTask, steps: &[PlanStep]) -> Result<(), GuardError>;
}

/// Composite guard combining neurorights, equality, augmentation fingerprint, and ROH limits.
pub struct CompositeGuards<N, E, A> {
    pub neurorights_guard: N,
    pub equality_guard: E,
    pub augfingerprint_guard: A,
    pub roh_global_ceiling: Bounded01,
}

impl<N, E, A> CompositeGuards<N, E, A> {
    pub fn new(
        neurorights_guard: N,
        equality_guard: E,
        augfingerprint_guard: A,
        roh_global_ceiling: Bounded01,
    ) -> Self {
        Self {
            neurorights_guard,
            equality_guard,
            augfingerprint_guard,
            roh_global_ceiling,
        }
    }

    fn check_global_roh_ceiling(&self, roh_value: f64) -> Result<(), GuardError> {
        if roh_value > self.roh_global_ceiling.into_inner() {
            return Err(GuardError::RoHExceeded(roh_value));
        }
        Ok(())
    }
}

impl<N, E, A> PrometheusGuards for CompositeGuards<N, E, A>
where
    N: DefaultNeurorightsGuard,
    E: EqualityPaymentGuard,
    A: AugFingerprintGuard,
{
    fn pre_plan_check(&self, task: &PrometheusTask) -> Result<(), GuardError> {
        self.neurorights_guard
            .check(&task.neurorights_env)
            .map_err(|e| GuardError::NeuroRights(e.to_string()))?;

        if task.service_class == "ServiceClassBasic" {
            self.equality_guard
                .check_basic_service(&task.equality_env)
                .map_err(|e| GuardError::Equality(e.to_string()))?;
        } else {
            self.equality_guard
                .check_premium_service(&task.equality_env)
                .map_err(|e| GuardError::Equality(e.to_string()))?;
        }

        self.augfingerprint_guard
            .check(&task.augfingerprint_env)
            .map_err(|e| GuardError::Equality(e.to_string()))?;

        let roh_target = task.roh_target.into_inner();
        self.check_global_roh_ceiling(roh_target)?;

        Ok(())
    }

    fn post_plan_check(&self, _task: &PrometheusTask, steps: &[PlanStep]) -> Result<(), GuardError> {
        let mut max_roh = 0.0_f64;

        for s in steps {
            let roh_val = s.estimated_roh.into_inner();
            if roh_val > max_roh {
                max_roh = roh_val;
            }
        }

        self.check_global_roh_ceiling(max_roh)?;

        Ok(())
    }
}
