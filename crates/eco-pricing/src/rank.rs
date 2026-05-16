// crates/eco-pricing/src/rank.rs
use serde::{Deserialize, Serialize};

use crate::models::{CoBenefit, EcoPricingShard};
use ecospine::KER;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneWeight {
    pub plane_id: String,  // "carbon", "energy", "biodiversity", etc.
    pub weight: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CandidateAction {
    pub action_id: String,
    pub intervention_id: String,
    pub units: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RankedAction {
    pub action_id: String,
    pub intervention_id: String,
    pub units: f64,
    pub total_cost: f64,
    pub total_weighted_benefit: f64,
    pub impact_cost_ratio: f64,
    pub ker: KER,
}

pub fn rank_actions(
    budget: f64,
    plane_weights: &[PlaneWeight],
    candidates: &[CandidateAction],
    pricing: &[EcoPricingShard],
) -> Vec<RankedAction> {
    let mut ranked: Vec<RankedAction> = Vec::new();

    for cand in candidates {
        if let Some(p) = pricing.iter().find(|p| p.intervention_id == cand.intervention_id) {
            let total_cost =
                (p.cost_per_unit.capex_per_unit + p.cost_per_unit.opex_per_unit) * cand.units;

            if total_cost > budget {
                continue;
            }

            let weighted_benefit =
                compute_weighted_benefit(&p.benefits, plane_weights, cand.units);

            let ratio = if total_cost > 0.0 {
                weighted_benefit / total_cost
            } else {
                0.0
            };

            ranked.push(RankedAction {
                action_id: cand.action_id.clone(),
                intervention_id: cand.intervention_id.clone(),
                units: cand.units,
                total_cost,
                total_weighted_benefit: weighted_benefit,
                impact_cost_ratio: ratio,
                ker: p.ker,
            });
        }
    }

    ranked.sort_by(|a, b| {
        b.impact_cost_ratio
            .partial_cmp(&a.impact_cost_ratio)
            .unwrap_or(std::cmp::Ordering::Equal)
    });

    ranked
}

fn compute_weighted_benefit(
    benefits: &[CoBenefit],
    plane_weights: &[PlaneWeight],
    units: f64,
) -> f64 {
    let mut sum = 0.0;

    for b in benefits {
        let plane_id = metric_plane(&b.metric);
        if let Some(w) = plane_weights.iter().find(|pw| pw.plane_id == plane_id) {
            sum += w.weight * b.mean * units;
        }
    }

    sum
}

fn metric_plane(metric: &str) -> String {
    match metric {
        m if m.starts_with("CO2_") => "carbon".to_string(),
        m if m.contains("biodiversity") => "biodiversity".to_string(),
        m if m.contains("cooling") => "energy".to_string(),
        _ => "restoration".to_string(),
    }
}
