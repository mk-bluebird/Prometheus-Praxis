// filename: econet_spine/src/eco_restoration_shard_block_21_25.rs

// This module is non-actuating and stays within the frozen ecosafety grammar.
// It encodes restoration_potential_i, nanoswarm decay tied to V_t, AIChatReputationShard,
// EducationRewardShard saturation handling, and LaserRestorationSimulator blast superposition.

use rusqlite::{Connection, params};
use std::f32;

// ---------- 21. restoration_potential_i for ecosystem service fee ----------

/// Compute restoration_potential_i as a function of K/E/R and residual V_t
/// using shardinstance and plane-specific hydraulics/canal metrics.
///
/// Semantics (per Phoenix canal basin window):
/// - High knowledge and eco-impact increase potential.
/// - High residual risk and canal velocity risk decrease potential.
/// - Result is in [0, 1].
pub fn compute_restoration_potential(
    k: f32,
    e: f32,
    r: f32,
    v_t: f32,
    v_t_ref: f32,
    r_canal: f32,
) -> f32 {
    // Clamp inputs to safe ranges.
    let k_clamped = k.min(1.0).max(0.0);
    let e_clamped = e.min(1.0).max(0.0);
    let r_clamped = r.min(1.0).max(0.0);
    let v_ratio = if v_t_ref > 0.0 {
        (v_t / v_t_ref).min(2.0)
    } else {
        1.0
    };
    let r_canal_clamped = r_canal.min(1.0).max(0.0);

    // Base potential from K and E, penalize R.
    let base = 0.5 * k_clamped + 0.5 * e_clamped - 0.3 * r_clamped;

    // Residual-based penalty for stressed basins.
    let residual_penalty = 0.25 * (v_ratio - 1.0);

    // Canal-velocity penalty: high r_canal lowers restoration potential.
    let canal_penalty = 0.25 * r_canal_clamped;

    let raw = base - residual_penalty - canal_penalty;
    raw.max(0.0).min(1.0)
}

/// SQL view definition tying shardinstance, vshardresidual, and vshardcanal
/// into a per-shard restoration_potential_i metric for Phoenix canal basins.
pub const DB_ECOSERVICE_RESTORATION_POTENTIAL_SQL: &str = r#"
-- filename: dbecoservice_restoration_potential.sql

-- View: vshard_restoration_potential
-- Purpose: expose restoration_potential_i per shardinstance window.
-- Assumes:
--   - vshardresidual(shardid, vt) is defined as in DR1.
--   - vshardcanal(shardid, rcanal) is defined as in CanalVelocityShard2026v1.
--   - shardinstance has kmetric, emetric, rmetric, region columns.

CREATE VIEW IF NOT EXISTS vshard_restoration_potential AS
SELECT
    si.shardid,
    si.nodeid,
    si.region,
    si.tstartutc,
    si.tendutc,
    si.kmetric,
    si.emetric,
    si.rmetric,
    vr.vt AS vt,
    vr_ref.vt AS vt_ref,
    vc.rcanal,
    -- Inline the same formula as compute_restoration_potential, using SQLite.
    CASE
        WHEN vt_ref <= 0.0 THEN
            MAX(0.0, MIN(1.0,
                (0.5 * MIN(MAX(si.kmetric, 0.0), 1.0)
               + 0.5 * MIN(MAX(si.emetric, 0.0), 1.0)
               - 0.3 * MIN(MAX(si.rmetric, 0.0), 1.0)
               - 0.25 * MIN(MAX(vc.rcanal, 0.0), 1.0)
            ))
        ELSE
            MAX(0.0, MIN(1.0,
                (0.5 * MIN(MAX(si.kmetric, 0.0), 1.0)
               + 0.5 * MIN(MAX(si.emetric, 0.0), 1.0)
               - 0.3 * MIN(MAX(si.rmetric, 0.0), 1.0)
               - 0.25 * MIN(MAX(vc.rcanal, 0.0), 1.0)
               - 0.25 * ((vr.vt / vt_ref) - 1.0)
            ))
    END AS restoration_potential_i
FROM shardinstance si
JOIN vshardresidual vr
  ON vr.shardid = si.shardid
JOIN vshardcanal vc
  ON vc.shardid = si.shardid
-- vt_ref: Phoenix canal reference residual, e.g. 2026-01-01 base window.
JOIN vshardresidual vr_ref
  ON vr_ref.region = si.region
 AND vr_ref.tstartutc = '2026-01-01T00:00:00Z'
 AND vr_ref.tendutc   = '2026-01-31T23:59:59Z'
WHERE si.region LIKE 'Phoenix-%';
"#;

/// Worked example for a Phoenix canal basin shard.
pub fn example_restoration_potential_phoenix_canal() -> f32 {
    let k = 0.94;
    let e = 0.91;
    let r = 0.12;
    let v_t_ref = 0.40;
    let v_t = 0.36;
    let r_canal = 0.25;
    compute_restoration_potential(k, e, r, v_t, v_t_ref, r_canal)
}

// ---------- 22. Nanoswarm eco-wealth decay tied to V_t ----------

/// Dynamic discount rate for nanoswarm eco-wealth as a function of residual V_t.
/// Uses a Lyapunov-style derivative proxy: higher V_t => higher decay.
/// dW/dt = -lambda(V_t) * W, solved as W(t) = W0 * exp(-lambda * t).
pub fn nanoswarm_discount_rate(v_t: f32, v_low: f32, v_high: f32,
                               lambda_min: f32, lambda_max: f32) -> f32 {
    let v_clamped = v_t.max(v_low).min(v_high);
    if v_high <= v_low {
        lambda_min
    } else {
        let alpha = (v_clamped - v_low) / (v_high - v_low);
        lambda_min + alpha * (lambda_max - lambda_min)
    }
}

/// Discrete-time update for nanoswarm wealth W_{t+1} given V_t and step dt.
/// Uses exponentiated decay with dynamic rate.
pub fn nanoswarm_wealth_next(w_t: f32,
                             v_t: f32,
                             v_low: f32,
                             v_high: f32,
                             lambda_min: f32,
                             lambda_max: f32,
                             dt_hours: f32) -> f32 {
    let lambda = nanoswarm_discount_rate(v_t, v_low, v_high, lambda_min, lambda_max);
    let decay = f32::exp(-lambda * dt_hours.max(0.0));
    w_t * decay
}

// ---------- 23. AIChatReputationShard ALN + SQL ----------

pub const AICHAT_REPUTATION_ALN: &str = r#"
-- filename: qpudatashards/particles/AIChatReputationShard2026v1.aln

schema AIChatReputationShard2026v1
  field shardid          Int        -- FK to shardinstance.shardid
  field chatinstanceid   String     -- stable ID for AI chat instance
  field model_tag        String     -- model/version identifier
  field lane             String     -- RESEARCH, EXPPROD, PROD
  field window_start_utc String     -- ISO8601
  field window_end_utc   String     -- ISO8601

  -- Event aggregates within window
  field n_sessions       Int
  field n_messages       Int
  field n_codegen        Int
  field n_nonactuating   Int        -- code verified non-actuating
  field n_violation      Int        -- ecosafety or lane violation events
  field n_corridor_warn  Int        -- near-miss corridor warnings
  field n_user_flags     Int        -- human complaints / flags
  field n_hexstamped_ok  Int        -- outputs with valid evidencehex and DID

  -- Quality scores [0,1]
  field kfactor          Float      -- knowledge factor for this chat instance
  field efactor          Float      -- eco-impact factor
  field rfactor          Float      -- risk-of-harm factor

  field reputation_score Float      -- derived [0,1]

  -- Governance / provenance
  field region           String
  field signingdid       String
  field evidencehex      String
  field created_utc      String
endschema
"#;

pub const DB_AICHAT_REPUTATION_SQL: &str = r#"
-- filename: dbaichatreputation.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS aichat_reputation (
  reputation_id     INTEGER PRIMARY KEY AUTOINCREMENT,
  shardid           INTEGER NOT NULL REFERENCES shardinstance(shardid) ON DELETE CASCADE,
  chatinstanceid    TEXT    NOT NULL,
  model_tag         TEXT    NOT NULL,
  lane              TEXT    NOT NULL CHECK (lane IN ('RESEARCH','EXPPROD','PROD')),
  window_start_utc  TEXT    NOT NULL,
  window_end_utc    TEXT    NOT NULL,
  n_sessions        INTEGER NOT NULL DEFAULT 0,
  n_messages        INTEGER NOT NULL DEFAULT 0,
  n_codegen         INTEGER NOT NULL DEFAULT 0,
  n_nonactuating    INTEGER NOT NULL DEFAULT 0,
  n_violation       INTEGER NOT NULL DEFAULT 0,
  n_corridor_warn   INTEGER NOT NULL DEFAULT 0,
  n_user_flags      INTEGER NOT NULL DEFAULT 0,
  n_hexstamped_ok   INTEGER NOT NULL DEFAULT 0,
  kfactor           REAL    NOT NULL,
  efactor           REAL    NOT NULL,
  rfactor           REAL    NOT NULL,
  reputation_score  REAL    NOT NULL,
  region            TEXT    NOT NULL,
  signingdid        TEXT    NOT NULL,
  evidencehex       TEXT    NOT NULL,
  created_utc       TEXT    NOT NULL
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_aichat_rep_window
  ON aichat_reputation(chatinstanceid, window_start_utc, window_end_utc);

-- Lane thresholds for AI chat instances, corridor-driven.
CREATE TABLE IF NOT EXISTS aichat_lane_threshold (
  lane          TEXT PRIMARY KEY,             -- RESEARCH, EXPPROD, PROD
  min_kfactor   REAL NOT NULL,
  min_efactor   REAL NOT NULL,
  max_rfactor   REAL NOT NULL,
  min_rep_score REAL NOT NULL
);

INSERT OR IGNORE INTO aichat_lane_threshold (lane, min_kfactor, min_efactor, max_rfactor, min_rep_score)
VALUES
  ('RESEARCH', 0.70, 0.70, 0.40, 0.40),
  ('EXPPROD',  0.85, 0.85, 0.20, 0.70),
  ('PROD',     0.90, 0.90, 0.13, 0.85);

-- Derived lane change decisions per chatinstance/window.
CREATE VIEW IF NOT EXISTS vaichat_lane_verdict AS
SELECT
  ar.reputation_id,
  ar.chatinstanceid,
  ar.model_tag,
  ar.lane AS current_lane,
  ar.kfactor,
  ar.efactor,
  ar.rfactor,
  ar.reputation_score,
  th.lane AS lane_profile,
  CASE
    WHEN ar.kfactor >= th.min_kfactor
     AND ar.efactor >= th.min_efactor
     AND ar.rfactor <= th.max_rfactor
     AND ar.reputation_score >= th.min_rep_score
    THEN 1 ELSE 0
  END AS admissible_for_lane
FROM aichat_reputation ar
JOIN aichat_lane_threshold th
  ON 1=1;

"#;

/// Compute reputation_score from underlying fields.
/// Example formula:
///   - Start from K/E/R.
///   - Reward non-actuating codegen and hex-stamped outputs.
///   - Penalize violations and user flags.
pub fn compute_aichat_reputation_score(
    kfactor: f32,
    efactor: f32,
    rfactor: f32,
    n_codegen: i32,
    n_nonactuating: i32,
    n_violation: i32,
    n_corridor_warn: i32,
    n_user_flags: i32,
    n_hexstamped_ok: i32,
) -> f32 {
    let k = kfactor.min(1.0).max(0.0);
    let e = efactor.min(1.0).max(0.0);
    let r = rfactor.min(1.0).max(0.0);

    let base = 0.4 * k + 0.4 * e - 0.3 * r;

    let codegen_safe_ratio = if n_codegen > 0 {
        (n_nonactuating as f32) / (n_codegen as f32)
    } else {
        1.0
    };
    let stamp_ratio = if n_codegen > 0 {
        (n_hexstamped_ok as f32) / (n_codegen as f32)
    } else {
        1.0
    };

    let penalty = 0.1 * (n_violation as f32)
                + 0.05 * (n_corridor_warn as f32)
                + 0.05 * (n_user_flags as f32);

    let bonus = 0.1 * codegen_safe_ratio + 0.1 * stamp_ratio;

    let raw = base + bonus - penalty;
    raw.max(0.0).min(1.0)
}

// ---------- 24. EducationRewardShard ΔK saturation handling ----------

/// Compute EducationRewardShard reward multiplier based on ΔK, with
/// saturation-aware maintenance band.
/// - delta_k: K_t - K_{t-1}
/// - k_current: current K
/// - k_maint_min: threshold above which maintenance rewards apply
pub fn education_reward_multiplier(
    delta_k: f32,
    k_current: f32,
    k_maint_min: f32,
) -> f32 {
    let k_clamped = k_current.min(1.0).max(0.0);
    let dk = delta_k;

    // Growth reward for positive ΔK, saturating.
    let growth = if dk > 0.0 {
        (0.5 * dk).min(0.20)
    } else {
        0.0
    };

    // Maintenance reward for high-K steady state.
    let maintenance = if k_clamped >= k_maint_min && dk.abs() < 0.01 {
        0.10
    } else {
        0.0
    };

    // Penalty for K degradation.
    let penalty = if dk < 0.0 {
        (-0.5 * dk).min(0.20)
    } else {
        0.0
    };

    let m = 1.0 + growth + maintenance - penalty;
    m.max(0.5).min(1.5)
}

pub const EDUCATION_REWARD_ALN: &str = r#"
-- filename: qpudatashards/particles/EducationRewardShard2026v1.aln

schema EducationRewardShard2026v1
  field shardid          Int
  field steward_did      String
  field window_start_utc String
  field window_end_utc   String
  field k_prev           Float
  field k_current        Float
  field delta_k          Float
  field k_maint_min      Float
  field reward_multiplier Float
  field evidencehex      String
  field signingdid       String
  field created_utc      String
endschema
"#;

// ---------- 25. LaserRestorationSimulator blast superposition ----------

/// Superposition rule for intersecting blastradius from multiple laser plans.
///
/// vblastradiusadjacent is modeled as:
///   v_adj = 1 - Π_i (1 - v_i_local),
/// where v_i_local are per-plan reflection risk contributions in [0,1]
/// at a given spatial cell.
pub fn superpose_vblastradius_adjacent(v_local: &[f32]) -> f32 {
    let mut product = 1.0_f32;
    for &v in v_local.iter() {
        let v_clamped = v.min(1.0).max(0.0);
        product *= 1.0 - v_clamped;
    }
    let v_adj = 1.0 - product;
    v_adj.min(1.0).max(0.0)
}

pub const DB_LASER_RESTORATION_SIMULATOR_SQL: &str = r#"
-- filename: dblaser_restoration_simulator.sql

PRAGMA foreign_keys = ON;

-- Per-laser-plan blastradius contribution per cell.
CREATE TABLE IF NOT EXISTS laser_plan_cell_risk (
  cell_id       INTEGER NOT NULL,
  laser_plan_id INTEGER NOT NULL,
  v_local       REAL    NOT NULL, -- 0..1 local reflection risk
  PRIMARY KEY (cell_id, laser_plan_id)
);

-- Aggregated vblastradiusadjacent per cell across concurrent plans.
CREATE VIEW IF NOT EXISTS vcell_vblastradiusadjacent AS
SELECT
  cell_id,
  1.0 - prod_nonref AS vblastradiusadjacent
FROM (
  SELECT
    cell_id,
    -- Π_i (1 - v_i_local) as product of non-reflection probability proxy.
    EXP(SUM(LOG(MAX(1.0 - MIN(MAX(v_local, 0.0), 1.0), 1e-6)))) AS prod_nonref
  FROM laser_plan_cell_risk
  GROUP BY cell_id
);
"#;

// ---------- Simple wiring helper to install SQL into a SQLite connection ----------

pub fn install_eco_restoration_block(conn: &Connection) -> rusqlite::Result<()> {
    conn.execute_batch(DB_ECOSERVICE_RESTORATION_POTENTIAL_SQL)?;
    conn.execute_batch(DB_AICHAT_REPUTATION_SQL)?;
    conn.execute_batch(DB_LASER_RESTORATION_SIMULATOR_SQL)?;
    Ok(())
}
