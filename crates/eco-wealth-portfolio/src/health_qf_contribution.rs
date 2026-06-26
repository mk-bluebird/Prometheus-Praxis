// filename: crates/eco-wealth-portfolio/src/health_qf_contribution.rs

use crate::qf_math::{compute_qf_total, compute_qf_weight, EcoCreditAttenuation, ResponsibilityScalar};
use rust_decimal::Decimal;
use rust_decimal_macros::dec;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Campaign identifier (e.g., QF-POOL-HEALTH-LABOR-001:round:local-id).
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct CampaignId(pub String);

/// Project identifier within a campaign.
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct ProjectId(pub String);

/// Account identifier (pseudonymous DLAccountId or scoped hash).
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct AccountId(pub String);

/// Single QF contribution row in memory, mirroring `healthqfcontribution`.
///
/// Intended mapping to SQLite:
/// - `campaignid`            TEXT
/// - `projectid`            TEXT
/// - `contributoraccountid` TEXT
/// - `amountdec`            TEXT (Decimal as string)
/// - `effectiveamountdec`   TEXT (Decimal as string)
/// - `responsibilityscalardec` TEXT
/// - `attenuationalphadec`  TEXT
/// - `cumulativecreditsdec` TEXT
/// - `tsutcms`              INTEGER
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthQFContribution {
    pub campaign_id: CampaignId,
    pub project_id: ProjectId,
    pub contributor_account_id: AccountId,
    pub amount: Decimal,
    pub effective_amount: Decimal,
    pub responsibility: ResponsibilityScalar,
    pub attenuation: EcoCreditAttenuation,
    pub ts_utc_ms: i64,
}

impl HealthQFContribution {
    /// Construct a contribution with computed effective_amount via QF kernel.
    pub fn new_with_kernel(
        campaign_id: CampaignId,
        project_id: ProjectId,
        contributor_account_id: AccountId,
        amount: Decimal,
        responsibility: ResponsibilityScalar,
        attenuation: EcoCreditAttenuation,
        ts_utc_ms: i64,
    ) -> Self {
        let effective_amount = compute_qf_weight(amount, responsibility, attenuation);
        HealthQFContribution {
            campaign_id,
            project_id,
            contributor_account_id,
            amount,
            effective_amount,
            responsibility,
            attenuation,
            ts_utc_ms,
        }
    }
}

/// Aggregated metrics per project within a campaign.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProjectAggregation {
    /// Raw sum of contributed amounts (for audit).
    pub sum_amount: Decimal,
    /// Sum of effective amounts (after responsibility and attenuation).
    pub sum_effective_amount: Decimal,
    /// QF total \((\sum \sqrt{\cdot})^2\) over effective amounts.
    pub qf_total: Decimal,
    /// Number of distinct contributors (for diversity metrics).
    pub distinct_contributors: usize,
}

/// Aggregated metrics per campaign.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CampaignAggregation {
    /// Per‑project QF aggregation.
    pub per_project: HashMap<ProjectId, ProjectAggregation>,
    /// Total QF score across all projects in this campaign.
    pub total_qf_score: Decimal,
}

impl CampaignAggregation {
    pub fn new() -> Self {
        CampaignAggregation {
            per_project: HashMap::new(),
            total_qf_score: Decimal::ZERO,
        }
    }
}

/// Aggregate a slice of contributions into per‑campaign, per‑project QF scores.
///
/// Invariants:
/// - Pure function; no I/O.
/// - Uses `compute_qf_total` to derive QF totals per project.
/// - Computes `total_qf_score` as the sum of all project QF totals.
pub fn aggregate_campaigns(
    contributions: &[HealthQFContribution],
) -> HashMap<CampaignId, CampaignAggregation> {
    // First: group contributions by (campaign, project).
    let mut grouped: HashMap<(CampaignId, ProjectId), Vec<&HealthQFContribution>> = HashMap::new();

    for c in contributions {
        let key = (c.campaign_id.clone(), c.project_id.clone());
        grouped.entry(key).or_default().push(c);
    }

    // Second: compute per‑project aggregates, then roll up into per‑campaign.
    let mut by_campaign: HashMap<CampaignId, CampaignAggregation> = HashMap::new();

    for ((campaign_id, project_id), group) in grouped.into_iter() {
        let mut sum_amount = Decimal::ZERO;
        let mut sum_effective = Decimal::ZERO;
        let mut effective_vec: Vec<Decimal> = Vec::with_capacity(group.len());
        let mut contributors: HashMap<AccountId, ()> = HashMap::new();

        for c in group {
            sum_amount += c.amount;
            sum_effective += c.effective_amount;
            effective_vec.push(c.effective_amount);
            contributors.insert(c.contributor_account_id.clone(), ());
        }

        let qf_total = compute_qf_total(&effective_vec);
        let distinct_contributors = contributors.len();

        let project_agg = ProjectAggregation {
            sum_amount,
            sum_effective_amount: sum_effective,
            qf_total,
            distinct_contributors,
        };

        let camp_entry = by_campaign.entry(campaign_id.clone()).or_insert_with(CampaignAggregation::new);
        camp_entry.per_project.insert(project_id, project_agg);
    }

    // Third: compute total_qf_score per campaign.
    for (_cid, camp_agg) in by_campaign.iter_mut() {
        let mut total = Decimal::ZERO;
        for (_pid, proj) in camp_agg.per_project.iter() {
            total += proj.qf_total;
        }
        camp_agg.total_qf_score = total;
    }

    by_campaign
}

/// Compute matching allocations for a single campaign given a fixed pool.
///
/// This is a pure helper; persisting the result as EcoModelAttestations is left
/// to the caller.
///
/// `matching_pool` is the pool size in the same units as `amount` (e.g., credits).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProjectMatch {
    pub project_id: ProjectId,
    pub qf_score: Decimal,
    pub matched_amount: Decimal,
}

pub fn allocate_matching_for_campaign(
    campaign_id: &CampaignId,
    contributions: &[HealthQFContribution],
    matching_pool: Decimal,
) -> Vec<ProjectMatch> {
    // Filter contributions for this campaign
    let filtered: Vec<HealthQFContribution> = contributions
        .iter()
        .filter(|c| &c.campaign_id == campaign_id)
        .cloned()
        .collect();

    let agg_map = aggregate_campaigns(&filtered);

    let camp_agg = match agg_map.get(campaign_id) {
        Some(a) => a,
        None => return Vec::new(),
    };

    let total_qf = if camp_agg.total_qf_score > dec!(0) {
        camp_agg.total_qf_score
    } else {
        return Vec::new();
    };

    let mut out = Vec::with_capacity(camp_agg.per_project.len());
    for (project_id, proj_agg) in camp_agg.per_project.iter() {
        let share = proj_agg.qf_total / total_qf;
        let matched = matching_pool * share;
        out.push(ProjectMatch {
            project_id: project_id.clone(),
            qf_score: proj_agg.qf_total,
            matched_amount: matched,
        });
    }

    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use rust_decimal_macros::dec;

    #[test]
    fn test_aggregate_campaigns_basic() {
        let c_id = CampaignId("C1".to_string());
        let p1 = ProjectId("P1".to_string());
        let p2 = ProjectId("P2".to_string());
        let acc1 = AccountId("A1".to_string());
        let acc2 = AccountId("A2".to_string());

        let r = ResponsibilityScalar::new(dec!(1));
        let att_zero = EcoCreditAttenuation::new(dec!(0), dec!(0));

        let c1 = HealthQFContribution::new_with_kernel(
            c_id.clone(),
            p1.clone(),
            acc1.clone(),
            dec!(1),
            r,
            att_zero,
            0,
        );
        let c2 = HealthQFContribution::new_with_kernel(
            c_id.clone(),
            p1.clone(),
            acc2.clone(),
            dec!(1),
            r,
            att_zero,
            1,
        );
        let c3 = HealthQFContribution::new_with_kernel(
            c_id.clone(),
            p2.clone(),
            acc1.clone(),
            dec!(4),
            r,
            att_zero,
            2,
        );

        let agg = aggregate_campaigns(&[c1, c2, c3]);
        let camp = agg.get(&c_id).expect("campaign present");
        assert_eq!(camp.per_project.len(), 2);
        assert!(camp.total_qf_score > dec!(0));
    }

    #[test]
    fn test_allocate_matching_for_campaign_splits_pool() {
        let c_id = CampaignId("C1".to_string());
        let p1 = ProjectId("P1".to_string());
        let p2 = ProjectId("P2".to_string());
        let acc = AccountId("A".to_string());
        let r = ResponsibilityScalar::new(dec!(1));
        let att_zero = EcoCreditAttenuation::new(dec!(0), dec!(0));

        // P1 effective ~ sqrt(1) = 1
        let c1 = HealthQFContribution::new_with_kernel(
            c_id.clone(),
            p1.clone(),
            acc.clone(),
            dec!(1),
            r,
            att_zero,
            0,
        );
        // P2 effective ~ sqrt(4) = 2
        let c2 = HealthQFContribution::new_with_kernel(
            c_id.clone(),
            p2.clone(),
            acc.clone(),
            dec!(4),
            r,
            att_zero,
            1,
        );

        let matches = allocate_matching_for_campaign(&c_id, &[c1, c2], dec!(300));
        assert_eq!(matches.len(), 2);

        // P2 should get more than P1
        let m1 = matches.iter().find(|m| m.project_id == p1).unwrap();
        let m2 = matches.iter().find(|m| m.project_id == p2).unwrap();
        assert!(m2.matched_amount > m1.matched_amount);
    }
}
