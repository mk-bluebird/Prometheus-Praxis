pub mod equations;
pub mod validate;

pub use crate::equations::{ConstraintEquation, ComparisonOp, TimePeriod};
pub use crate::validate::{ConstraintResult, validate_action};

/// Hydrology constraint state for risk evaluation.
#[derive(Debug, Clone)]
pub struct HydrologyConstraint {
    pub plane_id: String,
    pub constraint_level: f64,
}

impl HydrologyConstraint {
    /// Create a new hydrology constraint with the hydrology plane tag.
    pub fn new(constraint_level: f64) -> Self {
        Self {
            plane_id: "hydrology".to_string(),
            constraint_level,
        }
    }

    /// Convert hydrology constraints to a RiskVector for prometheus_praxis_spine integration.
    /// Stub implementation - tags PlaneId("hydrology") with placeholder values.
    pub fn to_risk_vector(&self) -> prometheus_praxis_spine::RiskVector {
        // TODO: Implement full mapping from HydrologyConstraint to RiskVector
        // This stub tags the hydrology plane as requested.
        prometheus_praxis_spine::RiskVector {
            plane_id: prometheus_praxis_spine::PlaneId::new("hydrology"),
            components: vec![self.constraint_level],
            magnitude: self.constraint_level.abs(),
        }
    }
}

/// Direct function as requested in the task.
pub fn to_risk_vector(constraint: &HydrologyConstraint) -> prometheus_praxis_spine::RiskVector {
    constraint.to_risk_vector()
}
