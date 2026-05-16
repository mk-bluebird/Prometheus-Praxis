// crates/hydrology-constraints/src/equations.rs

use serde::{Deserialize, Serialize};
use time::OffsetDateTime;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum TimePeriod {
    Day,
    Week,
    Month,
    Year,
    Custom {
        start: OffsetDateTime,
        end: OffsetDateTime,
    },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ComparisonOp {
    LessOrEqual,
    GreaterOrEqual,
    Equal,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ConstraintEquation {
    RechargeRate {
        aquifer: String,
        max_rate_m3_per_day: f64,
        confidence: f64,
    },
    WithdrawalLimit {
        well_id: String,
        limit_m3: f64,
        period: TimePeriod,
        confidence: f64,
    },
    GWRiskThreshold {
        region: String,
        gwr_max: f64,
        based_on_model: String,
        confidence: f64,
    },
    GenericLinear {
        lhs_coeffs: Vec<(String, f64)>,
        rhs: f64,
        op: ComparisonOp,
        confidence: f64,
    },
}

impl ConstraintEquation {
    pub fn confidence(&self) -> f64 {
        match self {
            ConstraintEquation::RechargeRate { confidence, .. } => *confidence,
            ConstraintEquation::WithdrawalLimit { confidence, .. } => *confidence,
            ConstraintEquation::GWRiskThreshold { confidence, .. } => *confidence,
            ConstraintEquation::GenericLinear { confidence, .. } => *confidence,
        }
    }
}
