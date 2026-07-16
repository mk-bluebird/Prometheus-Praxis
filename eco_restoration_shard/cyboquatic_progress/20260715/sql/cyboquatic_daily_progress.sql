-- filename: eco_restoration_shard/cyboquatic_progress/20260715/sql/cyboquatic_daily_progress.sql
-- purpose: Maintain cyboquatic_daily_progress.sqlite with daily_progress records and
--          a per-day INSERT, plus K,E,R and Phoenix hex evidence for 2026-07-15.

PRAGMA foreign_keys = ON;

-- Core daily_progress table for cyboquatic workloads (if not already created elsewhere).
CREATE TABLE IF NOT EXISTS daily_progress (
    progress_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    yyyymmdd      TEXT NOT NULL,
    domain        TEXT NOT NULL,
    subtask_id    TEXT NOT NULL,
    segment_id    TEXT NOT NULL,
    flow_m3s      REAL NOT NULL,
    head_loss_m   REAL NOT NULL,
    density_kgm3  REAL NOT NULL,
    g_ms2         REAL NOT NULL,
    energyreq_j   REAL NOT NULL,
    vt_before     REAL NOT NULL,
    vt_after      REAL NOT NULL,
    deltavt       REAL NOT NULL,
    k_factor      REAL NOT NULL,
    e_factor      REAL NOT NULL,
    r_factor      REAL NOT NULL,
    evidence_hex  TEXT NOT NULL,
    prior_pointer TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_daily_progress_date
    ON daily_progress(yyyymmdd);

CREATE INDEX IF NOT EXISTS idx_daily_progress_segment_time
    ON daily_progress(segment_id, yyyymmdd);

-- Daily INSERT for 2026-07-15 cyboquatic workload energyreqJ + ΔVt subtask.
INSERT INTO daily_progress (
    yyyymmdd,
    domain,
    subtask_id,
    segment_id,
    flow_m3s,
    head_loss_m,
    density_kgm3,
    g_ms2,
    energyreq_j,
    vt_before,
    vt_after,
    deltavt,
    k_factor,
    e_factor,
    r_factor,
    evidence_hex,
    prior_pointer
) VALUES (
    '20260715',
    'cyboquatic_workload',
    'PHX-CANAL-ENERGYREQDV-20260715',
    'PHX-CANAL-NODE-ENERGY-01',
    3.0,
    1.2,
    1000.0,
    9.81,
    35316.0,
    0.25,
    0.30,
    0.05,
    0.85,
    0.70,
    0.30,
    '0x20260715PHXENERGYREQDV',
    '0x20260714PHXPREVENERGYREQDV'
);

-- Governance-oriented commentary:
-- Next-step research queries:
-- 1) Tighten FLOW_SAFE_MAX, HEAD_SAFE_MAX, and ENERGY_SAFE_MAX based on Phoenix canal telemetry.
-- 2) Couple energyreq_j with biodegradable substrate drag reductions for carbon-negative routing.
-- 3) Extend K,E,R to include PFAS-specific residual coordinates in aligned qpudatashards.
