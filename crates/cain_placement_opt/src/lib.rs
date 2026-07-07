// ecorestoration_shard/crates/cain_placement_opt/src/lib.rs
#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};
use thiserror::Error;
use good_lp::{
    constraint, variable, variables, Expression, ProblemVariables, Solution, SolverModel,
    solvers::coin_cbc::CbcSolver,
};
use std::collections::{HashMap, HashSet};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct HviLocation {
    pub id: String,
    /// Normalized heat vulnerability score in [0, 1].
    pub heat_weight: f64,
}

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

impl CainSite {
    fn is_valid(&self) -> bool {
        self.fan_power_kw >= 0.0
            && self.water_m3_per_day >= 0.0
            && self.water_saving_m3_per_day >= 0.0
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CainEffect {
    pub hvi_id: String,
    pub site_id: String,
    /// 1 if the site can effectively serve this HVI location.
    pub coverage: u8,
    /// PM2.5 reduction at this HVI location from this site.
    pub pm_reduction: f64,
    /// NO2 reduction at this HVI location from this site.
    pub no2_reduction: f64,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct EcoWeights {
    pub w_water_saving: f64,
    pub w_pm: f64,
    pub w_no2: f64,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct ResourceBudgets {
    pub fan_power_kw_max: f64,
    pub water_m3_per_day_max: f64,
}

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

#[derive(Debug, Error)]
pub enum CainOptError {
    #[error("input data inconsistent: {0}")]
    InconsistentInput(String),
    #[error("solver error: {0}")]
    Solver(String),
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CainModelInputs {
    pub hvi_locations: Vec<HviLocation>,
    pub sites: Vec<CainSite>,
    pub effects: Vec<CainEffect>,
    pub eco_weights: EcoWeights,
    pub budgets: ResourceBudgets,
}

impl CainModelInputs {
    fn validate(&self) -> Result<(), CainOptError> {
        if self.hvi_locations.is_empty() {
            return Err(CainOptError::InconsistentInput(
                "no HVI locations provided".to_string(),
            ));
        }
        if self.sites.is_empty() {
            return Err(CainOptError::InconsistentInput(
                "no CAIN sites provided".to_string(),
            ));
        }
        if self.effects.is_empty() {
            return Err(CainOptError::InconsistentInput(
                "no CAIN effects provided".to_string(),
            ));
        }
        if self.budgets.fan_power_kw_max < 0.0 || self.budgets.water_m3_per_day_max < 0.0 {
            return Err(CainOptError::InconsistentInput(
                "budgets must be non-negative".to_string(),
            ));
        }
        for s in &self.sites {
            if !s.is_valid() {
                return Err(CainOptError::InconsistentInput(format!(
                    "site {} has negative parameter",
                    s.id
                )));
            }
        }

        let hvi_ids: HashSet<_> = self.hvi_locations.iter().map(|h| h.id.as_str()).collect();
        let site_ids: HashSet<_> = self.sites.iter().map(|s| s.id.as_str()).collect();

        for e in &self.effects {
            if !hvi_ids.contains(e.hvi_id.as_str()) {
                return Err(CainOptError::InconsistentInput(format!(
                    "effect references unknown hvi_id {}",
                    e.hvi_id
                )));
            }
            if !site_ids.contains(e.site_id.as_str()) {
                return Err(CainOptError::InconsistentInput(format!(
                    "effect references unknown site_id {}",
                    e.site_id
                )));
            }
            if e.coverage > 1 {
                return Err(CainOptError::InconsistentInput(format!(
                    "effect coverage must be 0 or 1, got {}",
                    e.coverage
                )));
            }
            if e.pm_reduction < 0.0 || e.no2_reduction < 0.0 {
                return Err(CainOptError::InconsistentInput(format!(
                    "negative PM/NO2 reduction for site {} at hvi {}",
                    e.site_id, e.hvi_id
                )));
            }
        }

        Ok(())
    }
}

struct SolutionWithVars {
    solution: Solution,
    x: Vec<variable::Binary>,
}

impl SolutionWithVars {
    fn value(&self, v: variable::Binary) -> f64 {
        self.solution.value(v)
    }
    fn var(&self, j: usize) -> variable::Binary {
        self.x[j]
    }
}

/// Compute Pareto-approximate placements by sweeping lambda over [0,1].
///
/// `lambda_grid` is a slice of eco-weights between 0 and 1 (e.g., &[0.0, 0.25, 0.5, 0.75, 1.0]).
pub fn compute_pareto_front(
    inputs: &CainModelInputs,
    lambda_grid: &[f64],
) -> Result<Vec<ParetoPoint>, CainOptError> {
    inputs.validate()?;

    let site_index: HashMap<&str, usize> = inputs
        .sites
        .iter()
        .enumerate()
        .map(|(j, s)| (s.id.as_str(), j))
        .collect();

    let hvi_index: HashMap<&str, usize> = inputs
        .hvi_locations
        .iter()
        .enumerate()
        .map(|(i, h)| (h.id.as_str(), i))
        .collect();

    let n_sites = inputs.sites.len();
    let mut eco_coeff: Vec<f64> = vec![0.0; n_sites];
    let mut equity_coeff: Vec<f64> = vec![0.0; n_sites];

    for (j, site) in inputs.sites.iter().enumerate() {
        eco_coeff[j] += inputs.eco_weights.w_water_saving * site.water_saving_m3_per_day;
    }

    for eff in &inputs.effects {
        let j = *site_index
            .get(eff.site_id.as_str())
            .ok_or_else(|| CainOptError::InconsistentInput("missing site in index".to_string()))?;
        let i = *hvi_index
            .get(eff.hvi_id.as_str())
            .ok_or_else(|| CainOptError::InconsistentInput("missing hvi in index".to_string()))?;
        let h_weight = inputs.hvi_locations[i].heat_weight;
        let cov = eff.coverage as f64;

        eco_coeff[j] += cov
            * (inputs.eco_weights.w_pm * eff.pm_reduction
                + inputs.eco_weights.w_no2 * eff.no2_reduction);
        equity_coeff[j] += cov * h_weight;
    }

    let mut results: Vec<ParetoPoint> = Vec::new();

    for &lambda in lambda_grid {
        if !(0.0..=1.0).contains(&lambda) {
            return Err(CainOptError::InconsistentInput(format!(
                "lambda must be in [0,1], got {}",
                lambda
            )));
        }

        let (solution, z_eco, z_equity) =
            solve_weighted_bip(inputs, &eco_coeff, &equity_coeff, lambda)?;

        let selected_sites: Vec<String> = inputs
            .sites
            .iter()
            .enumerate()
            .filter_map(|(j, s)| {
                let xj = solution.value(solution.var(j));
                if xj > 0.5 {
                    Some(s.id.clone())
                } else {
                    None
                }
            })
            .collect();

        let scalar_obj = lambda * z_eco + (1.0 - lambda) * z_equity;

        results.push(ParetoPoint {
            lambda_eco: lambda,
            selected_sites,
            eco_impact: z_eco,
            heat_equity: z_equity,
            scalar_objective: scalar_obj,
        });
    }

    Ok(results)
}

fn solve_weighted_bip(
    inputs: &CainModelInputs,
    eco_coeff: &[f64],
    equity_coeff: &[f64],
    lambda: f64,
) -> Result<(SolutionWithVars, f64, f64), CainOptError> {
    let n_sites = inputs.sites.len();

    let mut vars = variables!();
    let x: Vec<variable::Binary> = (0..n_sites)
        .map(|j| vars.add(variable().binary().name(format!("x_{}", j))))
        .collect();

    let mut expr_fan: Expression = 0.0.into();
    let mut expr_water: Expression = 0.0.into();
    let mut expr_eco: Expression = 0.0.into();
    let mut expr_equity: Expression = 0.0.into();

    for (j, site) in inputs.sites.iter().enumerate() {
        expr_fan = expr_fan + site.fan_power_kw * x[j];
        expr_water = expr_water + site.water_m3_per_day * x[j];
        expr_eco = expr_eco + eco_coeff[j] * x[j];
        expr_equity = expr_equity + equity_coeff[j] * x[j];
    }

    let scalar_obj: Expression = lambda * expr_eco.clone() + (1.0 - lambda) * expr_equity.clone();

    let mut problem = vars.maximise(scalar_obj).using(CbcSolver::new());

    problem = problem
        .with(constraint!(expr_fan.clone() <= inputs.budgets.fan_power_kw_max))
        .with(constraint!(
            expr_water.clone() <= inputs.budgets.water_m3_per_day_max
        ));

    let solution = problem
        .solve()
        .map_err(|e| CainOptError::Solver(format!("MILP solver failed: {:?}", e)))?;

    let mut z_eco = 0.0;
    let mut z_equity = 0.0;
    for j in 0..n_sites {
        let xj = solution.value(x[j]);
        if xj > 0.5 {
            z_eco += eco_coeff[j];
            z_equity += equity_coeff[j];
        }
    }

    Ok((SolutionWithVars { solution, x }, z_eco, z_equity))
}
