// File: crates/bioscale_evolution_cli/src/tx_builder.rs
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EcoContributionPlan {
    pub estimated_boot: u64,
    pub eco_multiplier: f64,
    pub risk_ratio: f64,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum ContributionError {
    InvalidInput,
    AmountOverflow,
}

pub fn build_eco_contribution_plan(
    gas_cost: u64,
    eco_multiplier_base: f64,
    r_axis: f64,
) -> Result<Option<EcoContributionPlan>, ContributionError> {
    if !eco_multiplier_base.is_finite() || !r_axis.is_finite() || r_axis < 0.0 || r_axis > 1.0 {
        return Err(ContributionError::InvalidInput);
    }
    if gas_cost == 0 || eco_multiplier_base <= 0.0 || r_axis <= 0.0 {
        return Ok(None);
    }

    let estimate = (gas_cost as f64) * eco_multiplier_base * r_axis;
    if !estimate.is_finite() || estimate < 0.0 || estimate > u64::MAX as f64 {
        return Err(ContributionError::AmountOverflow);
    }

    Ok(Some(EcoContributionPlan {
        estimated_boot: estimate.round() as u64,
        eco_multiplier: eco_multiplier_base,
        risk_ratio: r_axis,
    }))
}
