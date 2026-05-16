use serde::{Deserialize, Serialize};

use crate::equations::{ComparisonOp, ConstraintEquation};

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
pub struct ConstraintViolation {
    pub message: String,
    pub confidence: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConstraintWarning {
    pub message: String,
    pub confidence: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConstraintResult {
    pub passed: bool,
    pub violations: Vec<ConstraintViolation>,
    pub warnings: Vec<ConstraintWarning>,
    pub effective_confidence: f64,
}

pub fn validate_action(
    constraints: &[ConstraintEquation],
    action: &ProposedAction,
    min_confidence_for_strict: f64,
) -> ConstraintResult {
    let mut passed = true;
    let mut violations = Vec::new();
    let mut warnings = Vec::new();
    let mut effective_confidence = 1.0;

    for c in constraints {
        let conf = c.confidence();
        if conf < effective_confidence {
            effective_confidence = conf;
        }

        match c {
            ConstraintEquation::RechargeRate {
                aquifer,
                max_rate_m3_per_day,
                confidence,
            } => {
                if let Some(act_aquifer) = &action.aquifer {
                    if act_aquifer == aquifer && action.recharge_m3_per_day > *max_rate_m3_per_day {
                        if *confidence >= min_confidence_for_strict {
                            passed = false;
                            violations.push(ConstraintViolation {
                                message: format!(
                                    "RechargeRate exceeded for aquifer {}: {} > {}",
                                    aquifer, action.recharge_m3_per_day, max_rate_m3_per_day
                                ),
                                confidence: *confidence,
                            });
                        } else {
                            warnings.push(ConstraintWarning {
                                message: format!(
                                    "Low-confidence RechargeRate exceeded for aquifer {}: {} > {}",
                                    aquifer, action.recharge_m3_per_day, max_rate_m3_per_day
                                ),
                                confidence: *confidence,
                            });
                        }
                    }
                }
            }
            ConstraintEquation::WithdrawalLimit {
                well_id,
                limit_m3,
                confidence,
                ..
            } => {
                if let Some(act_well) = &action.well_id {
                    if act_well == well_id && action.withdrawal_m3 > *limit_m3 {
                        if *confidence >= min_confidence_for_strict {
                            passed = false;
                            violations.push(ConstraintViolation {
                                message: format!(
                                    "WithdrawalLimit exceeded for well {}: {} > {}",
                                    well_id, action.withdrawal_m3, limit_m3
                                ),
                                confidence: *confidence,
                            });
                        } else {
                            warnings.push(ConstraintWarning {
                                message: format!(
                                    "Low-confidence WithdrawalLimit exceeded for well {}: {} > {}",
                                    well_id, action.withdrawal_m3, limit_m3
                                ),
                                confidence: *confidence,
                            });
                        }
                    }
                }
            }
            ConstraintEquation::GWRiskThreshold {
                region,
                gwr_max,
                confidence,
                ..
            } => {
                if &action.region == region {
                    let gw_use = action.withdrawal_m3 - action.recharge_m3_per_day;
                    if gw_use > *gwr_max {
                        if *confidence >= min_confidence_for_strict {
                            passed = false;
                            violations.push(ConstraintViolation {
                                message: format!(
                                    "GWRiskThreshold exceeded for region {}: {} > {}",
                                    region, gw_use, gwr_max
                                ),
                                confidence: *confidence,
                            });
                        } else {
                            warnings.push(ConstraintWarning {
                                message: format!(
                                    "Low-confidence GWRiskThreshold exceeded for region {}: {} > {}",
                                    region, gw_use, gwr_max
                                ),
                                confidence: *confidence,
                            });
                        }
                    }
                }
            }
            ConstraintEquation::GenericLinear {
                lhs_coeffs,
                rhs,
                op,
                confidence,
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
                    if *confidence >= min_confidence_for_strict {
                        passed = false;
                        violations.push(ConstraintViolation {
                            message: format!(
                                "GenericLinear constraint violated: lhs = {}, rhs = {}",
                                lhs_sum, rhs
                            ),
                            confidence: *confidence,
                        });
                    } else {
                        warnings.push(ConstraintWarning {
                            message: format!(
                                "Low-confidence GenericLinear constraint violated: lhs = {}, rhs = {}",
                                lhs_sum, rhs
                            ),
                            confidence: *confidence,
                        });
                    }
                }
            }
        }
    }

    ConstraintResult {
        passed,
        violations,
        warnings,
        effective_confidence,
    }
}
