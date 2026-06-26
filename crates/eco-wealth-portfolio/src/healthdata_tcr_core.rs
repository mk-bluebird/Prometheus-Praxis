// filename: crates/eco-wealth-portfolio/src/healthdata_tcr_core.rs

use chrono::{DateTime, Utc};
use rust_decimal::Decimal;
use rust_decimal::prelude::ToPrimitive;
use rusqlite::{params, Connection, Result as SqlResult};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::time::{SystemTime, UNIX_EPOCH};
use thiserror::Error;

pub type Address = String;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, Hash)]
pub struct BrainDid {
    pub did: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum LaborSourceKind {
    HealthDevice,
    ClinicExport,
    EnvSensor,
    AiChatSummary,
    ManualEntry,
    OtherAccountActivity,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DataLaborEvent {
    pub id: i64,
    pub subject_brain: BrainDid,
    pub source_kind: LaborSourceKind,
    pub related_account_id: Option<String>,
    pub created_at_utc: DateTime<Utc>,
    pub note: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum CurationStatus {
    Pending,
    Accepted,
    Rejected,
    Slashed,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum DatasetCategory {
    WearableVitals,
    EegNeuro,
    EhrSummary,
    LabResultsAggregate,
    EnvExposure,
    ClimateHealthLinked,
    OtherHealthRestricted,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthDataset {
    pub dataset_cid: String,
    pub contributor: Address,
    pub subject_brain: BrainDid,
    pub curator: Address,
    pub stake_amount: Decimal,
    pub status: CurationStatus,
    pub category: DatasetCategory,
    pub quality_score: Decimal,
    pub neurorights_safe: bool,
    pub msg_publish_ref: i64,
    pub labor_event_ids: Vec<i64>,
    pub responsibility_scalar_snap: Decimal,
    pub ker_anchor_id: i64,
    pub eco_credit_reward_locked: Decimal,
    pub eco_credit_reward_vested: Decimal,
    pub health_qf_eligible: bool,
    pub health_qf_round_id: Option<i64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthDatasetChallenge {
    pub id: i64,
    pub dataset_cid: String,
    pub challenger: Address,
    pub challenge_stake: Decimal,
    pub alleged_violation: String,
    pub evidence_cid: String,
    pub status: String,
    pub slash_ratio_curator: Option<Decimal>,
    pub slash_ratio_challenger: Option<Decimal>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthDataLaborCredit {
    pub id: i64,
    pub contributor: Address,
    pub dataset_cid: String,
    pub eco_credit_earned: Decimal,
    pub gas_discount_bps: u32,
    pub non_transferable: bool,
    pub remaining_gas_discount: Decimal,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthQFPool {
    pub id: i64,
    pub round_label: String,
    pub matching_pool_boot: Decimal,
    pub matching_pool_ecocredit: Decimal,
    pub round_start_utc: DateTime<Utc>,
    pub round_end_utc: DateTime<Utc>,
    pub ker_anchor_id: i64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthQFCampaign {
    pub id: i64,
    pub pool_id: i64,
    pub ecocampaign_id: i64,
    pub creator: Address,
    pub is_healthcare: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthQFContribution {
    pub id: i64,
    pub campaign_id: i64,
    pub contributor: Address,
    pub contributed_boot: Decimal,
    pub contributed_ecocredit: Decimal,
    pub cumulative_ecocredit_snap: Decimal,
    pub responsibility_scalar_snap: Decimal,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthQFWeights {
    pub sqrt_boot_sum: Decimal,
    pub qf_weight: Decimal,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoModelAttestation {
    pub ecocampaign_id: i64,
    pub round_label: String,
    pub matched_boot: Decimal,
    pub matched_ecocredit: Decimal,
    pub ker_anchor_id: i64,
}

#[derive(Debug, Error)]
pub enum HealthTcrError {
    #[error("value out of bounds: {0}")]
    OutOfBounds(&'static str),
    #[error("dataset status invalid for operation: {0}")]
    InvalidStatus(&'static str),
    #[error("curator mismatch")]
    CuratorMismatch,
    #[error("labor events missing")]
    LaborEventsMissing,
}

#[derive(Debug, Clone)]
pub struct Config {
    pub base_eco_credit_rate: f64,
    pub responsibility_alpha: f64,
    pub plutocracy_alpha: f64,
}

pub struct HealthDataTcrService {
    conn: Connection,
    pub config: Config,
}

impl HealthDataTcrService {
    pub fn new(db_path: &str, config: Config) -> SqlResult<Self> {
        let conn = Connection::open(db_path)?;
        Ok(Self { conn, config })
    }

    pub fn issue_eco_credits_for_labor_event(
        &self,
        brain_id: i64,
        labor_event_id: i64,
        effort_score_local: f64,
        responsibility_scalar: f64,
    ) -> SqlResult<()> {
        let now = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("time went backwards")
            .as_secs()
            .to_string();

        let cumulative: f64 = self.conn.query_row(
            "SELECT IFNULL(MAX(cumulative_eco_credit), 0.0)
             FROM eco_credit_ledger WHERE brain_id = ?1",
            params![brain_id],
            |row| row.get(0),
        )?;

        let amount = self.config.base_eco_credit_rate * effort_score_local * responsibility_scalar;
        let new_cumulative = cumulative + amount;

        self.conn.execute(
            "INSERT INTO eco_credit_ledger
             (brain_id, labor_event_id, issued_at, amount,
              non_transferable, responsibility_scalar, cumulative_eco_credit)
             VALUES (?1, ?2, ?3, ?4, 1, ?5, ?6)",
            params![
                brain_id,
                labor_event_id,
                now,
                amount,
                responsibility_scalar,
                new_cumulative
            ],
        )?;

        Ok(())
    }

    pub fn record_qf_allocation(
        &self,
        project_id: i64,
        brain_id: i64,
        allocated_eco_credits: f64,
    ) -> SqlResult<()> {
        let now = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("time went backwards")
            .as_secs()
            .to_string();

        let (responsibility_scalar, cumulative_eco_credit): (f64, f64) = self.conn.query_row(
            "SELECT responsibility_scalar, cumulative_eco_credit
             FROM eco_credit_ledger
             WHERE brain_id = ?1
             ORDER BY id DESC LIMIT 1",
            params![brain_id],
            |row| Ok((row.get(0)?, row.get(1)?)),
        )?;

        let alpha = self.config.plutocracy_alpha;
        let attenuation = 1.0_f64 / (1.0 + alpha * (1.0 + cumulative_eco_credit).ln());
        let effective_weight = allocated_eco_credits * responsibility_scalar * attenuation;

        self.conn.execute(
            "INSERT INTO qf_allocation
             (project_id, brain_id, allocated_eco_credits,
              responsibility_scalar, effective_weight, allocated_at)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
            params![
                project_id,
                brain_id,
                allocated_eco_credits,
                responsibility_scalar,
                effective_weight,
                now
            ],
        )?;

        Ok(())
    }

    pub fn finalize_qf_round(&self, round_id: i64) -> SqlResult<()> {
        let mut stmt = self.conn.prepare(
            "SELECT project_id, brain_id, effective_weight
             FROM qf_allocation",
        )?;

        let allocation_iter = stmt.query_map([], |row| {
            Ok((
                row.get::<_, i64>(0)?,
                row.get::<_, i64>(1)?,
                row.get::<_, f64>(2)?,
            ))
        })?;

        let mut per_project: HashMap<i64, Vec<f64>> = HashMap::new();

        for row in allocation_iter {
            let (project_id, _brain_id, effective_weight) = row?;
            per_project.entry(project_id).or_default().push(effective_weight);
        }

        let matching_pool: f64 = self.conn.query_row(
            "SELECT matching_pool FROM qf_round WHERE id = ?1",
            params![round_id],
            |row| row.get(0),
        )?;

        let mut project_scores: HashMap<i64, f64> = HashMap::new();
        let mut total_score = 0.0_f64;

        for (project_id, weights) in &per_project {
            let sum_sqrt: f64 = weights.iter().map(|w| w.sqrt()).sum();
            let score = sum_sqrt * sum_sqrt;
            project_scores.insert(*project_id, score);
            total_score += score;
        }

        for (project_id, score) in project_scores {
            let share = if total_score > 0.0 {
                score / total_score
            } else {
                0.0
            };
            let matched_amount = matching_pool * share;
            let total_effective_support: f64 =
                per_project.get(&project_id).map(|v| v.iter().sum()).unwrap_or(0.0);

            self.conn.execute(
                "INSERT INTO qf_round_result
                 (round_id, project_id, matched_amount, total_effective_support)
                 VALUES (?1, ?2, ?3, ?4)",
                params![round_id, project_id, matched_amount, total_effective_support],
            )?;
        }

        Ok(())
    }
}

pub fn new_data_labor_event(
    id: i64,
    subject_brain: BrainDid,
    source_kind: LaborSourceKind,
    related_account_id: Option<String>,
    created_at_utc: DateTime<Utc>,
    note: String,
) -> DataLaborEvent {
    DataLaborEvent {
        id,
        subject_brain,
        source_kind,
        related_account_id,
        created_at_utc,
        note,
    }
}

pub fn submit_health_dataset(
    curator: Address,
    dataset_cid: String,
    subject_brain: BrainDid,
    contributor: Address,
    msg_publish_id: i64,
    category: DatasetCategory,
    stake_amount: Decimal,
    initial_quality_score: Decimal,
    labor_event_ids: Vec<i64>,
    responsibility_scalar_snap: Decimal,
    ker_anchor_id: i64,
) -> Result<HealthDataset, HealthTcrError> {
    if stake_amount < Decimal::ZERO {
        return Err(HealthTcrError::OutOfBounds("stake_amount < 0"));
    }
    if initial_quality_score < Decimal::ZERO || initial_quality_score > Decimal::ONE {
        return Err(HealthTcrError::OutOfBounds("quality_score not in [0,1]"));
    }
    if labor_event_ids.is_empty() {
        return Err(HealthTcrError::LaborEventsMissing);
    }
    if responsibility_scalar_snap < Decimal::ZERO || responsibility_scalar_snap > Decimal::ONE {
        return Err(HealthTcrError::OutOfBounds(
            "responsibility_scalar_snap not in [0,1]",
        ));
    }

    Ok(HealthDataset {
        dataset_cid,
        contributor,
        subject_brain,
        curator,
        stake_amount,
        status: CurationStatus::Pending,
        category,
        quality_score: initial_quality_score,
        neurorights_safe: true,
        msg_publish_ref: msg_publish_id,
        labor_event_ids,
        responsibility_scalar_snap,
        ker_anchor_id,
        eco_credit_reward_locked: Decimal::ZERO,
        eco_credit_reward_vested: Decimal::ZERO,
        health_qf_eligible: false,
        health_qf_round_id: None,
    })
}

pub fn accept_health_dataset(
    curator: &Address,
    dataset: &HealthDataset,
    eco_reward_locked: Decimal,
) -> Result<HealthDataset, HealthTcrError> {
    if &dataset.curator != curator {
        return Err(HealthTcrError::CuratorMismatch);
    }
    if dataset.status != CurationStatus::Pending {
        return Err(HealthTcrError::InvalidStatus("not pending"));
    }
    if !dataset.neurorights_safe {
        return Err(HealthTcrError::InvalidStatus("neurorights unsafe"));
    }

    let mut updated = dataset.clone();
    updated.status = CurationStatus::Accepted;
    updated.eco_credit_reward_locked = eco_reward_locked;
    Ok(updated)
}

pub fn reject_health_dataset(
    curator: &Address,
    dataset: &HealthDataset,
) -> Result<HealthDataset, HealthTcrError> {
    if &dataset.curator != curator {
        return Err(HealthTcrError::CuratorMismatch);
    }
    if dataset.status != CurationStatus::Pending {
        return Err(HealthTcrError::InvalidStatus("not pending"));
    }

    let mut updated = dataset.clone();
    updated.status = CurationStatus::Rejected;
    Ok(updated)
}

pub fn record_health_data_labor_credit(
    dataset: &HealthDataset,
    credit_id: i64,
    eco_credit_earned: Decimal,
    gas_discount_bps: u32,
) -> Result<HealthDataLaborCredit, HealthTcrError> {
    if dataset.status != CurationStatus::Accepted {
        return Err(HealthTcrError::InvalidStatus("dataset not accepted"));
    }
    if eco_credit_earned < Decimal::ZERO {
        return Err(HealthTcrError::OutOfBounds("eco_credit_earned < 0"));
    }
    if gas_discount_bps > 10_000 {
        return Err(HealthTcrError::OutOfBounds("gas_discount_bps > 10000"));
    }

    Ok(HealthDataLaborCredit {
        id: credit_id,
        contributor: dataset.contributor.clone(),
        dataset_cid: dataset.dataset_cid.clone(),
        eco_credit_earned,
        gas_discount_bps,
        non_transferable: true,
        remaining_gas_discount: eco_credit_earned,
    })
}

pub fn consume_gas_discount(
    credit: &HealthDataLaborCredit,
    gas_fee: Decimal,
) -> Result<(HealthDataLaborCredit, Decimal), HealthTcrError> {
    if !credit.non_transferable {
        return Err(HealthTcrError::InvalidStatus("credit must be non-transferable"));
    }
    if gas_fee < Decimal::ZERO {
        return Err(HealthTcrError::OutOfBounds("gas_fee < 0"));
    }

    let bps = Decimal::from(credit.gas_discount_bps);
    let ten_thousand = Decimal::from(10_000u32);
    let max_discount = gas_fee * bps / ten_thousand;

    let available = credit.remaining_gas_discount;
    let discount = if available >= max_discount {
        max_discount
    } else {
        available
    };

    let mut updated = credit.clone();
    updated.remaining_gas_discount = available - discount;

    let adjusted_fee = gas_fee - discount;
    Ok((updated, adjusted_fee))
}

pub fn open_challenge(
    challenge_id: i64,
    challenger: Address,
    dataset: &HealthDataset,
    challenge_stake: Decimal,
    alleged_violation: String,
    evidence_cid: String,
) -> Result<HealthDatasetChallenge, HealthTcrError> {
    if dataset.status != CurationStatus::Accepted {
        return Err(HealthTcrError::InvalidStatus(
            "can only challenge accepted datasets",
        ));
    }
    if challenge_stake < Decimal::ZERO {
        return Err(HealthTcrError::OutOfBounds("challenge_stake < 0"));
    }

    Ok(HealthDatasetChallenge {
        id: challenge_id,
        dataset_cid: dataset.dataset_cid.clone(),
        challenger,
        challenge_stake,
        alleged_violation,
        evidence_cid,
        status: "OPEN".to_string(),
        slash_ratio_curator: None,
        slash_ratio_challenger: None,
    })
}

pub fn resolve_challenge_slash_curator(
    dataset: &HealthDataset,
    mut challenge: HealthDatasetChallenge,
    slash_ratio_curator: Decimal,
    slash_ratio_challenger: Decimal,
) -> Result<(HealthDataset, HealthDatasetChallenge), HealthTcrError> {
    if challenge.status != "OPEN" {
        return Err(HealthTcrError::InvalidStatus("challenge not open"));
    }
    for (val, name) in [
        (slash_ratio_curator, "slash_ratio_curator"),
        (slash_ratio_challenger, "slash_ratio_challenger"),
    ] {
        if val < Decimal::ZERO || val > Decimal::ONE {
            return Err(HealthTcrError::OutOfBounds(name));
        }
    }

    let mut updated_dataset = dataset.clone();
    updated_dataset.status = CurationStatus::Slashed;

    challenge.status = "RESOLVED_SLASH".to_string();
    challenge.slash_ratio_curator = Some(slash_ratio_curator);
    challenge.slash_ratio_challenger = Some(slash_ratio_challenger);

    Ok((updated_dataset, challenge))
}

pub fn compute_qf_weight(contributions: &[HealthQFContribution]) -> HealthQFWeights {
    let half = Decimal::new(5, 1);
    let mut sqrt_sum = Decimal::ZERO;

    for c in contributions {
        let base = c.contributed_boot + c.contributed_ecocredit;
        let scaled = base * (half + half * c.responsibility_scalar_snap);
        let f = scaled.to_f64().unwrap_or(0.0).max(0.0);
        let sqrt_term = Decimal::from_f64(f.sqrt()).unwrap_or(Decimal::ZERO);
        sqrt_sum += sqrt_term;
    }

    let qf_weight = sqrt_sum * sqrt_sum;
    HealthQFWeights {
        sqrt_boot_sum: sqrt_sum,
        qf_weight,
    }
}

pub fn allocate_qf_matching(
    pool: &HealthQFPool,
    campaigns: &[HealthQFCampaign],
    contributions_by_campaign: &[Vec<HealthQFContribution>],
) -> Vec<EcoModelAttestation> {
    assert_eq!(
        campaigns.len(),
        contributions_by_campaign.len(),
        "campaigns and contributions_by_campaign must have same length"
    );

    let mut weights = Vec::with_capacity(campaigns.len());
    let mut total_weight = Decimal::ZERO;

    for contribs in contributions_by_campaign {
        let w = compute_qf_weight(contribs);
        total_weight += w.qf_weight;
        weights.push(w.qf_weight);
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

        attestations.push(EcoModelAttestation {
            ecocampaign_id: campaign.ecocampaign_id,
            round_label: pool.round_label.clone(),
            matched_boot,
            matched_ecocredit,
            ker_anchor_id: pool.ker_anchor_id,
        });
    }

    attestations
}
