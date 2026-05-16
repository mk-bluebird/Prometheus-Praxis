use serde::{Deserialize, Serialize};
use time::OffsetDateTime;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum TimePeriod {
    Day,
    Week,
    Month,
    Year,
    Custom { start: OffsetDateTime, end: OffsetDateTime },
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
    },
    GWRiskThreshold {
        region: String,
        gwr_max: f64,
        based_on_model: String,
    },
    GenericLinear {
        lhs_coeffs: Vec<(String, f64)>, // variable name, coefficient
        rhs: f64,
        op: ComparisonOp,
    },
}
