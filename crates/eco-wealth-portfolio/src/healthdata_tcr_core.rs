// filename: crates/eco-wealth-portfolio/src/healthdata_tcr_core.rs

use chrono::{DateTime, Utc};
use rust_decimal::prelude::*;
use rust_decimal::Decimal;
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

/// High‑level status of a health dataset in the TCR.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum DatasetStatus {
    Pending,
    Accepted,
    Rejected,
    Slashed,
}

/// Legacy curation status kept for compatibility with existing records.
/// Prefer `DatasetStatus` in new core logic.
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

/// Submission payload for a new dataset entering the TCR core.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthDatasetSubmission {
    pub dataset_id: String,
    pub contributor_account_id: String,
    pub curator_account_id: String,
    pub neurorights_policy_id: String,
    pub corridor_id: String,
    pub evidencemode: String,
    pub neurorights_safe: bool,
    pub quality_score: Decimal,
    pub stake_amount: Decimal,
    pub labor_event_ids: Vec<String>,
    pub created_at_utc: String,
}

/// Stored state for a dataset within the pure core.
/// This is what adapters persist into SQLite.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthDatasetState {
    pub dataset_id: String,
    pub contributor_account_id: String,
    pub curator_account_id: String,
    pub neurorights_policy_id: String,
    pub corridor_id: String,
    pub evidencemode: String,
    pub neurorights_safe: bool,
    pub quality_score: Decimal,
    pub stake_amount: Decimal,
    pub status: DatasetStatus,
    pub challenge_open: bool,
    pub labor_event_ids: Vec<String>,
    pub created_at_utc: String,
    pub updated_at_utc: String,
}

/// Context passed in by the caller; no I/O inside the core.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TcrDecisionContext {
    pub epoch_index: u64,
    pub min_quality_score: Decimal,
    pub min_stake_credits: Decimal,
    pub max_quality_score: Decimal,
    pub allow_non_neurorights_safe: bool,
}

/// Curator identifier wrapper for clarity.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CuratorId(pub String);

/// Error type for pure HealthData TCR operations.
#[derive(Debug, Error)]
pub enum HealthDatasetTcrError {
    #[error("dataset already exists: {dataset_id}")]
    DatasetAlreadyExists { dataset_id: String },

    #[error("dataset not found: {dataset_id}")]
    DatasetNotFound { dataset_id: String },

    #[error(
        "invalid state transition for dataset {dataset_id}: {from:?} -> {to:?}"
    )]
    InvalidStateTransition {
        dataset_id: String,
        from: DatasetStatus,
        to: DatasetStatus,
    },

    #[error("neurorights violation for dataset {dataset_id}: {detail}")]
    NeurorightsViolation { dataset_id: String, detail: String },

    #[error(
        "quality score out of bounds for dataset {dataset_id}: {quality_score}"
    )]
    QualityScoreOutOfBounds {
        dataset_id: String,
        quality_score: Decimal,
    },

    #[error(
        "insufficient quality score for dataset {dataset_id}: \
         {quality_score} < {required}"
    )]
    InsufficientQualityScore {
        dataset_id: String,
        quality_score: Decimal,
        required: Decimal,
    },

    #[error(
        "insufficient stake for dataset {dataset_id}: {stake_amount} < {required}"
    )]
    InsufficientStake {
        dataset_id: String,
        stake_amount: Decimal,
        required: Decimal,
    },

    #[error("challenge window still open for dataset {dataset_id}")]
    ChallengeWindowOpen { dataset_id: String },

    #[error("challenge window already closed for dataset {dataset_id}")]
    ChallengeWindowClosed { dataset_id: String },

    #[error(
        "curator mismatch for dataset {dataset_id}: expected {expected}, got {actual}"
    )]
    CuratorMismatch {
        dataset_id: String,
        expected: String,
        actual: String,
    },

    #[error("internal invariant violation: {detail}")]
    InvariantViolation { detail: String },
}

/// Clamp a Decimal into [lo, hi].
fn clamp_decimal(x: Decimal, lo: Decimal, hi: Decimal) -> Decimal {
    if x < lo {
        lo
    } else if x > hi {
        hi
    } else {
        x
    }
}

/// Pure constructor for a new dataset submission into the TCR.
pub fn submit_health_dataset_core(
    submission: HealthDatasetSubmission,
    existing: Option<HealthDatasetState>,
    ctx: &TcrDecisionContext,
    now_utc: &str,
) -> Result<HealthDatasetState, HealthDatasetTcrError> {
    if existing.is_some() {
        return Err(HealthDatasetTcrError::DatasetAlreadyExists {
            dataset_id: submission.dataset_id.clone(),
        });
    }

    if !submission.neurorights_safe && !ctx.allow_non_neurorights_safe {
        return Err(HealthDatasetTcrError::NeurorightsViolation {
            dataset_id: submission.dataset_id.clone(),
            detail: "neurorights_safe=false not allowed in this corridor".into(),
        });
    }

    if submission.neurorights_safe {
        let mode = submission.evidencemode.as_str();
        if mode != "HASHONLY" && mode != "PSEUDONYMOUSFEATURES" {
            return Err(HealthDatasetTcrError::NeurorightsViolation {
                dataset_id: submission.dataset_id.clone(),
                detail: "neurorights_safe dataset must be HASHONLY or PSEUDONYMOUSFEATURES"
                    .into(),
            });
        }
    }

    let q = clamp_decimal(
        submission.quality_score,
        Decimal::ZERO,
        ctx.max_quality_score,
    );
    if q != submission.quality_score {
        return Err(HealthDatasetTcrError::QualityScoreOutOfBounds {
            dataset_id: submission.dataset_id.clone(),
            quality_score: submission.quality_score,
        });
    }
    if q < ctx.min_quality_score {
        return Err(HealthDatasetTcrError::InsufficientQualityScore {
            dataset_id: submission.dataset_id.clone(),
            quality_score: q,
            required: ctx.min_quality_score,
        });
    }

    if submission.stake_amount < ctx.min_stake_credits {
        return Err(HealthDatasetTcrError::InsufficientStake {
            dataset_id: submission.dataset_id.clone(),
            stake_amount: submission.stake_amount,
            required: ctx.min_stake_credits,
        });
    }

    Ok(HealthDatasetState {
        dataset_id: submission.dataset_id,
        contributor_account_id: submission.contributor_account_id,
        curator_account_id: submission.curator_account_id,
        neurorights_policy_id: submission.neurorights_policy_id,
        corridor_id: submission.corridor_id,
        evidencemode: submission.evidencemode,
        neurorights_safe: submission.neurorights_safe,
        quality_score: q,
        stake_amount: submission.stake_amount,
        status: DatasetStatus::Pending,
        challenge_open: true,
        labor_event_ids: submission.labor_event_ids,
        created_at_utc: submission.created_at_utc,
        updated_at_utc: now_utc.to_owned(),
    })
}

/// Pure transition from Pending → Accepted.
pub fn accept_health_dataset_core(
    existing: &HealthDatasetState,
    curator_id: CuratorId,
    ctx: &TcrDecisionContext,
    now_utc: &str,
) -> Result<HealthDatasetState, HealthDatasetTcrError> {
    if existing.status != DatasetStatus::Pending {
        return Err(HealthDatasetTcrError::InvalidStateTransition {
            dataset_id: existing.dataset_id.clone(),
            from: existing.status,
            to: DatasetStatus::Accepted,
        });
    }

    if existing.curator_account_id != curator_id.0 {
        return Err(HealthDatasetTcrError::CuratorMismatch {
            dataset_id: existing.dataset_id.clone(),
            expected: existing.curator_account_id.clone(),
            actual: curator_id.0,
        });
    }

    if !existing.neurorights_safe && !ctx.allow_non_neurorights_safe {
        return Err(HealthDatasetTcrError::NeurorightsViolation {
            dataset_id: existing.dataset_id.clone(),
            detail: "attempted to accept neurorights-unsafe dataset".into(),
        });
    }

    let mut out = existing.clone();
    out.status = DatasetStatus::Accepted;
    out.challenge_open = true;
    out.updated_at_utc = now_utc.to_owned();
    Ok(out)
}

/// Pure transition for rejection / slashing.
pub fn reject_health_dataset_core(
    existing: &HealthDatasetState,
    curator_id: CuratorId,
    reason_code: &str,
    now_utc: &str,
) -> Result<HealthDatasetState, HealthDatasetTcrError> {
    let target_status = match existing.status {
        DatasetStatus::Pending => DatasetStatus::Rejected,
        DatasetStatus::Accepted => DatasetStatus::Slashed,
        s => {
            return Err(HealthDatasetTcrError::InvalidStateTransition {
                dataset_id: existing.dataset_id.clone(),
                from: s,
                to: DatasetStatus::Rejected,
            })
        }
    };

    if existing.curator_account_id != curator_id.0 {
        return Err(HealthDatasetTcrError::CuratorMismatch {
            dataset_id: existing.dataset_id.clone(),
            expected: existing.curator_account_id.clone(),
            actual: curator_id.0,
        });
    }

    if reason_code.trim().is_empty() {
        return Err(HealthDatasetTcrError::InvariantViolation {
            detail: "reject_health_dataset requires non-empty reason_code".into(),
        });
    }

    let mut out = existing.clone();
    out.status = target_status;
    out.challenge_open = false;
    out.updated_at_utc = now_utc.to_owned();
    Ok(out)
}

/// DataLaborEvent is kept for backwards‑compatible labor history.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DataLaborEvent {
    pub id: i64,
    pub subject_brain: BrainDid,
    pub source_kind: LaborSourceKind,
    pub related_account_id: Option<String>,
    pub created_at_utc: DateTime<Utc>,
    pub note: String,
}

/// Legacy on‑chain/TCR dataset view; kept where CID and KER anchors are required.
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

/// Stateful service wiring the pure core to SQLite.
/// This is non‑pure and should be kept out of the ALN‑pure core.
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

pub fn submit_health_dataset_legacy(
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

pub fn accept_health_dataset_legacy(
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

pub fn reject_health_dataset_legacy(
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
