use thiserror::Error;
use crate::planner::PlanStep;
use crate::types::PrometheusTask;
use transhuman_rights_core::DefaultNeurorightsGuard;
use equality_payment_guard::EqualityPaymentGuard;
use aug_fingerprint_guard::AugFingerprintGuard;
use bioscale_metrics::Bounded01;

#[derive(Debug, Error)]
pub enum GuardError {
    #[error("Neurorights violation: {0}")]
    NeuroRights(String),
    #[error("Equality violation: {0}")]
    Equality(String),
    #[error("ROH ceiling exceeded")]
    RoHExceeded,
}

pub trait PrometheusGuards {
    fn pre_plan_check(&self, task: &PrometheusTask) -> Result<(), GuardError>;
    fn post_plan_check(&self, task: &PrometheusTask, steps: &[PlanStep]) -> Result<(), GuardError>;
}

pub struct CompositeGuards<N, E, A> {
    pub neurorights_guard: N,
    pub equality_guard: E,
    pub augfingerprint_guard: A,
    pub roh_global_ceiling: Bounded01,
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
        }

        Ok(())
    }

    fn post_plan_check(&self, _task: &PrometheusTask, steps: &[PlanStep]) -> Result<(), GuardError> {
        let mut max_roh = 0.0_f32;
        for s in steps {
            if s.estimated_roh > max_roh {
                max_roh = s.estimated_roh;
            }
        }
        if max_roh > self.roh_global_ceiling.into_inner() {
            return Err(GuardError::RoHExceeded);
        }
        Ok(())
    }
}
