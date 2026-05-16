use serde::{Deserialize, Serialize};

use crate::equations::{ComparisonOp, ConstraintEquation, TimePeriod};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProposedAction {
    pub region: String,
    pub aquifer: Option<String>,
    pub well_id: Option<String>,
    pub withdrawal_m3: f64,
    pub recharge_m3_per_day: f64,
    pub variables: Vec<(String, f64)>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConstraintResult {
    pub passed: bool,
    pub violations: Vec<String>,
}

pub fn validate_action(
    constraints: &[ConstraintEquation],
    action: &ProposedAction,
) -> ConstraintResult {
    let mut passed = true;
    let mut violations = Vec::new();

    for c in constraints {
        match c {
            ConstraintEquation::RechargeRate {
                aquifer,
                max_rate_m3_per_day,
                ..
            } => {
                if let Some(act_aquifer) = &action.aquifer {
                    if act_aquifer == aquifer && action.recharge_m3_per_day > *max_rate_m3_per_day {
                        passed = false;
                        violations.push(format!(
                            "RechargeRate exceeded for aquifer {}: {} > {}",
                            aquifer, action.recharge_m3_per_day, max_rate_m3_per_day
                        ));
                    }
                }
            }
            ConstraintEquation::WithdrawalLimit {
                well_id,
                limit_m3,
                ..
            } => {
                if let Some(act_well) = &action.well_id {
                    if act_well == well_id && action.withdrawal_m3 > *limit_m3 {
                        passed = false;
                        violations.push(format!(
                            "WithdrawalLimit exceeded for well {}: {} > {}",
                            well_id, action.withdrawal_m3, limit_m3
                        ));
                    }
                }
            }
            ConstraintEquation::GWRiskThreshold {
                region,
                gwr_max,
                ..
            } => {
                if &action.region == region {
                    let gw_use = action.withdrawal_m3 - action.recharge_m3_per_day;
                    if gw_use > *gwr_max {
                        passed = false;
                        violations.push(format!(
                            "GWRiskThreshold exceeded for region {}: {} > {}",
                            region, gw_use, gwr_max
                        ));
                    }
                }
            }
            ConstraintEquation::GenericLinear {
                lhs_coeffs,
                rhs,
                op,
            } => {
                let mut lhs_sum = 0.0;
                for (name, coeff) in lhs_coeffs {
                    if let Some((_, val)) = action.variables.iter().find(|(n, _)| n == name) {
                        lhs_sum += coeff * val;
                    }
                }
                let ok = match op {
                    ComparisonOp::LessOrEqual => lhs_sum <= *rhs,
                    ComparisonOp::GreaterOrEqual => lhs_sum >= *rhs,
                    ComparisonOp::Equal => (lhs_sum - rhs).abs() < 1e-9,
                };
                if !ok {
                    passed = false;
                    violations.push(format!(
                        "GenericLinear constraint violated: lhs = {}, rhs = {}",
                        lhs_sum, rhs
                    ));
                }
            }
        }
    }

    ConstraintResult { passed, violations }
}
