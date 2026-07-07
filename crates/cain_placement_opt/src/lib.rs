// ecorestoration_shard/crates/cain_placement_opt/src/lib.rs
#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};
use thiserror::Error;
use good_lp::{
    variable, variables, Expression, ProblemVariables, Solution, SolverModel,
    constraint, solvers::coin_cbc::CbcSolver,
};

/// HVI-tagged location index and attributes.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct HviLocation {
    pub id: String,
    /// Normalized heat vulnerability score in [0, 1].
    pub heat_weight: f64,
}

/// Candidate CAIN site with resource and eco-parameters.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CainSite {
    pub id: String,
    /// Fan power draw in kW when operating.
    pub fan_power_kw: f64,
    /// Water usage in m3/day when operating.
    pub water_m3_per_day: f64,
    /// Water-saving coefficient in m3/day (offset).
    pub water_saving_m3_per_day: f64,
}

/// Pairwise effect of site j on HVI location i.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CainEffect {
    pub hvi_id: String,
    pub site_id: String,
    /// 1 if the site can effectively serve this HVI location.
    pub coverage: u8,
    /// PM2.5 reduction (e.g., µg/m3) at this HVI location from this site.
    pub pm_reduction: f64,
    /// NO2 reduction (e.g., µg/m3) at this HVI location from this site.
    pub no2_reduction: f64,
}

/// Scalarization weights for eco-impact components.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct EcoWeights {
    pub w_water_saving: f64,
    pub w_pm: f64,
    pub w_no2: f64,
}

/// Resource budgets shared across all sites.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct ResourceBudgets {
    pub fan_power_kw_max: f64,
    pub water_m3_per_day_max: f64,
}

/// Single Pareto point result for a given lambda.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ParetoPoint {
    pub lambda_eco: f64,
    /// Optimal binary decisions x_j for this scalarization.
    pub selected_sites: Vec<String>,
    /// Eco-impact objective value Z_eco.
    pub eco_impact: f64,
    /// Heat-equity objective value Z_equity.
    pub heat_equity: f64,
    /// Combined scalar objective lambda*Z_eco + (1-lambda)*Z_equity.
    pub scalar_objective: f64,
}

/// Errors from the optimization kernel.
#[derive(Debug, Error)]
pub enum CainOptError {
    #[error("input data inconsistent: {0}")]
    InconsistentInput(String),
    #[error("solver error: {0}")]
    Solver(String),
}
