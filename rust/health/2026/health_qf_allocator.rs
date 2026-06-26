// Path: rust/health/2026/health_qf_allocator.rs
// Role: Off-chain quadratic funding allocator for health campaigns, aligned with HealthDataTCR2026v1.

use std::time::SystemTime;

use rust_decimal::Decimal;
use rust_decimal::prelude::ToPrimitive;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthQFPool {
    pub pool_id: i64,
    pub round_label: String,
    pub matching_pool_boot: Decimal,
    pub matching_pool_ecocredit: Decimal,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthQFCampaign {
    pub campaign_id: i64,
    pub pool_id: i64,
    pub ecocampaign_id: i64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthQFContribution {
    pub contribution_id: i64,
    pub campaign_id: i64,
    pub contributor_address: String,
    pub contributed_boot: Decimal,
    pub contributed_ecocredit: Decimal,
    pub responsibility_scalar_snap: Decimal,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoModelAttestation {
    pub ecocampaign_id: i64,
    pub round_label: String,
    pub matched_boot: Decimal,
    pub matched_ecocredit: Decimal,
    pub generated_at: SystemTime,
}

fn sqrt_decimal(value: Decimal) -> Decimal {
    let f = value.to_f64().unwrap_or(0.0);
    let r = f.sqrt();
    Decimal::from_f64(r).unwrap_or(Decimal::ZERO)
}

fn compute_qf_weight(contributions: &[HealthQFContribution]) -> Decimal {
    let mut sqrt_sum = Decimal::ZERO;
    for c in contributions {
        let base = c.contributed_boot + c.contributed_ecocredit;
        let half = Decimal::new(5, 1); // 0.5
        let rs = c.responsibility_scalar_snap;
        let scaled = base * (half + half * rs);
        let sqrt_term = sqrt_decimal(scaled);
        sqrt_sum += sqrt_term;
    }
    sqrt_sum * sqrt_sum
}

pub fn allocate_qf_matching(
    pool: &HealthQFPool,
    campaigns: &[HealthQFCampaign],
    contributions_by_campaign: &[Vec<HealthQFContribution>],
) -> Vec<EcoModelAttestation> {
    let mut weights: Vec<Decimal> = Vec::with_capacity(campaigns.len());
    let mut total_weight = Decimal::ZERO;

    for contribs in contributions_by_campaign {
        let w = compute_qf_weight(contribs);
        total_weight += w;
        weights.push(w);
    }

    let mut attestations = Vec::with_capacity(campaigns.len());
    for (idx, campaign) in campaigns.iter().enumerate() {
        let w = weights[idx];
        let share = if total_weight > Decimal::ZERO {
            w / total_weight
        } else {
            Decimal::ZERO
        };

        let matched_boot = pool.matching_pool_boot * share;
        let matched_ecocredit = pool.matching_pool_ecocredit * share;

        let att = EcoModelAttestation {
            ecocampaign_id: campaign.ecocampaign_id,
            round_label: pool.round_label.clone(),
            matched_boot,
            matched_ecocredit,
            generated_at: SystemTime::now(),
        };
        attestations.push(att);
    }

    attestations
}
