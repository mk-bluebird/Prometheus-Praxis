// filename: ecokarma.rs
// destination: crates/eco-restoration-governance/src/ecokarma.rs
#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

/// Quadratic stake direction (repair vs drain).
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum StakeDirection {
    Repair,
    Drain,
}

impl StakeDirection {
    pub fn from_i32(v: i32) -> Option<Self> {
        match v {
            1 => Some(StakeDirection::Repair),
            -1 => Some(StakeDirection::Drain),
            _ => None,
        }
    }

    pub fn to_i32(self) -> i32 {
        match self {
            StakeDirection::Repair => 1,
            StakeDirection::Drain => -1,
        }
    }
}

/// In-memory representation of a row from ecorepairstakepool.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoRepairStakePool {
    pub poolid: i64,
    pub poolname: String,
    pub evolutionepochid: i64,
    pub totalstake: f64,
    pub totaleffectivestake: f64,
}

/// In-memory representation of a row from ecorepairstake.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoRepairStake {
    pub stakeid: i64,
    pub poolid: i64,
    pub actordid: String,
    pub stakeamount: f64,
    pub effectiveweight: f64,
    pub direction: StakeDirection,
}

/// Boot reward event from bootrewardevent.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BootRewardEvent {
    pub bootrewardid: i64,
    pub evolutionepochid: i64,
    pub actordid: String,
    pub rbaseline: f64,
    pub kbaseline: f64,
    pub ebaseline: f64,
    pub tokensawarded: f64,
    pub ecosplitjson: String,
    pub rdeltatarget: f64,
    pub auditedclosed: bool,
}

/// Responsibility audit row from bootrewardresponsibilityaudit.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BootRewardResponsibilityAudit {
    pub auditid: i64,
    pub bootrewardid: i64,
    pub auditwindowstartutc: String,
    pub auditwindowendutc: String,
    pub robserved: f64,
    pub rdeltaobserved: f64,
    pub targetmet: bool,
    pub rtrustobserved: f64,
}

/// KER residual snapshot from kerresidualsnapshot.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerResidualSnapshot {
    pub snapshotid: i64,
    pub region: String,
    pub kernelid: String,
    pub epoch: i64,
    pub kmean: f64,
    pub emean: f64,
    pub rmean: f64,
    pub renergy: f64,
    pub rcarbon: f64,
    pub rbiodiversity: f64,
    pub rtopology: f64,
    pub rtrust: f64,
    pub reff: f64,
    pub windowstartutc: String,
    pub windowendutc: String,
}

/// Summary for boot reward audits, suitable for CI or AI-chat surfaces.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BootRewardAuditSummary {
    pub bootrewardid: i64,
    pub actordid: String,
    pub epoch: i64,
    pub tokensawarded: f64,
    pub rdeltatarget: f64,
    pub rdeltaobserved: f64,
    pub rtrustobserved: f64,
    pub targetmet: bool,
    pub auditedclosed: bool,
}

/// Summary of pool-level quadratic effective stakes, separated by direction.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PoolEffectiveStakeSummary {
    pub poolid: i64,
    pub poolname: String,
    pub epoch: i64,
    pub total_stake_repair: f64,
    pub total_stake_drain: f64,
    pub total_effective_repair: f64,
    pub total_effective_drain: f64,
}

/// Compute sqrt-based quadratic effective weight for a non-negative stake amount.
pub fn quadratic_effective_weight(stakeamount: f64) -> f64 {
    if stakeamount <= 0.0 {
        0.0
    } else {
        stakeamount.sqrt()
    }
}

/// Recompute effective weights and pool aggregates from raw stakes.
/// This is pure and non-actuating; callers are responsible for persisting updates.
pub fn recompute_pool_effective_stake(
    pool: &EcoRepairStakePool,
    stakes: &[EcoRepairStake],
) -> PoolEffectiveStakeSummary {
    let mut total_stake_repair = 0.0;
    let mut total_stake_drain = 0.0;
    let mut total_effective_repair = 0.0;
    let mut total_effective_drain = 0.0;

    for s in stakes.iter().filter(|s| s.poolid == pool.poolid) {
        let w = quadratic_effective_weight(s.stakeamount);
        match s.direction {
            StakeDirection::Repair => {
                total_stake_repair += s.stakeamount;
                total_effective_repair += w;
            }
            StakeDirection::Drain => {
                total_stake_drain += s.stakeamount;
                total_effective_drain += w;
            }
        }
    }

    PoolEffectiveStakeSummary {
        poolid: pool.poolid,
        poolname: pool.poolname.clone(),
        epoch: pool.evolutionepochid,
        total_stake_repair,
        total_stake_drain,
        total_effective_repair,
        total_effective_drain,
    }
}

/// Evaluate whether a boot reward met its responsibility delta target,
/// given the latest audit. This is a pure helper for CI and governance.
pub fn evaluate_boot_reward_audit(
    event: &BootRewardEvent,
    audits: &[BootRewardResponsibilityAudit],
) -> Option<BootRewardAuditSummary> {
    let latest = audits
        .iter()
        .filter(|a| a.bootrewardid == event.bootrewardid)
        .max_by_key(|a| &a.auditwindowendutc)?;

    Some(BootRewardAuditSummary {
        bootrewardid: event.bootrewardid,
        actordid: event.actordid.clone(),
        epoch: event.evolutionepochid,
        tokensawarded: event.tokensawarded,
        rdeltatarget: event.rdeltatarget,
        rdeltaobserved: latest.rdeltaobserved,
        rtrustobserved: latest.rtrustobserved,
        targetmet: latest.targetmet,
        auditedclosed: event.auditedclosed,
    })
}

/// Extract a KER triad plus effective residual for use in eco-wealth and reward views.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerWindowSummary {
    pub region: String,
    pub kernelid: String,
    pub epoch: i64,
    pub kmean: f64,
    pub emean: f64,
    pub rmean: f64,
    pub rtrust: f64,
    pub reff: f64,
    pub windowstartutc: String,
    pub windowendutc: String,
}

/// Map kerresidualsnapshot rows into KerWindowSummary for CI/AI-chat.
pub fn to_ker_window_summaries(
    snapshots: &[KerResidualSnapshot],
) -> Vec<KerWindowSummary> {
    snapshots
        .iter()
        .map(|k| KerWindowSummary {
            region: k.region.clone(),
            kernelid: k.kernelid.clone(),
            epoch: k.epoch,
            kmean: k.kmean,
            emean: k.emean,
            rmean: k.rmean,
            rtrust: k.rtrust,
            reff: k.reff,
            windowstartutc: k.windowstartutc.clone(),
            windowendutc: k.windowendutc.clone(),
        })
        .collect()
}
