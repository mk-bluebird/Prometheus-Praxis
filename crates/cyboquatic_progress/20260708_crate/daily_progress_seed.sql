-- filename: eco_restoration_shard/crates/cyboquatic_progress/20260708_crate/daily_progress_seed.sql

PRAGMA foreign_keys = ON;

-- Create daily_progress table if not present (for environments using plain SQL migration). [file:33]
CREATE TABLE IF NOT EXISTS daily_progress (
    progress_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    yyyymmdd           TEXT NOT NULL,
    crate_id           TEXT NOT NULL,
    domain             TEXT NOT NULL,
    subtask_id         TEXT NOT NULL,
    node_id            TEXT NOT NULL,
    sample_id          TEXT NOT NULL,
    timestamp_utc      TEXT NOT NULL,
    bod_mg_l           REAL NOT NULL,
    tss_mg_l           REAL NOT NULL,
    cec_cmol_per_kg    REAL NOT NULL,
    r_bod              REAL NOT NULL,
    r_tss              REAL NOT NULL,
    r_cec              REAL NOT NULL,
    vt_before          REAL NOT NULL,
    vt_after           REAL NOT NULL,
    delta_vt           REAL NOT NULL,
    k_factor           REAL NOT NULL,
    e_factor           REAL NOT NULL,
    r_factor           REAL NOT NULL,
    evidence_hex       TEXT NOT NULL,
    prior_crate_id     TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_daily_progress_date
    ON daily_progress (yyyymmdd);

CREATE INDEX IF NOT EXISTS idx_daily_progress_node_time
    ON daily_progress (node_id, timestamp_utc);

-- Seed one Phoenix drainage-decay sample for 2026-07-08, matching the Rust normalization logic. [file:31][file:19]
-- Corridor-aligned example: moderately low BOD/TSS, mid-band CEC, vt_before chosen to allow slight improvement.

INSERT INTO daily_progress (
    yyyymmdd,
    crate_id,
    domain,
    subtask_id,
    node_id,
    sample_id,
    timestamp_utc,
    bod_mg_l,
    tss_mg_l,
    cec_cmol_per_kg,
    r_bod,
    r_tss,
    r_cec,
    vt_before,
    vt_after,
    delta_vt,
    k_factor,
    e_factor,
    r_factor,
    evidence_hex,
    prior_crate_id
)
VALUES (
    '20260708',
    'cyboquatic_drainagedecay_20260708',
    'drainagedecay',
    'PHX-CANAL-DF-2026-07-08',
    'PHX-CANAL-NODE-DF-01',
    'PHX-DF-SAMPLE-0001',
    '2026-07-08T23:31:00Z',
    3.0,               -- BOD mg/L
    12.0,              -- TSS mg/L
    10.0,              -- CEC cmol(+)/kg
    3.0 / 5.0,         -- r_bod (BOD_SAFE_MAX_MG_L)
    12.0 / 20.0,       -- r_tss (TSS_SAFE_MAX_MG_L)
    0.0,               -- r_cec (within safe band)
    0.25,              -- vt_before (example residual)
    0.4 * (3.0 / 5.0) * (3.0 / 5.0)
        + 0.35 * (12.0 / 20.0) * (12.0 / 20.0)
        + 0.25 * 0.0 * 0.0,  -- vt_after, matching Rust residual
    (0.4 * (3.0 / 5.0) * (3.0 / 5.0)
        + 0.35 * (12.0 / 20.0) * (12.0 / 20.0)
        + 0.25 * 0.0 * 0.0) - 0.25, -- delta_vt
    0.95 - 0.3 * MAX(3.0/5.0, 12.0/20.0), -- k_factor (upper-bounded but consistent with Rust formula)
    0.95 - (0.4 * (3.0 / 5.0) * (3.0 / 5.0)
        + 0.35 * (12.0 / 20.0) * (12.0 / 20.0)
        + 0.25 * 0.0 * 0.0),          -- e_factor (no ΔVt penalty here since example corridor)
    (0.4 * (3.0 / 5.0) * (3.0 / 5.0)
        + 0.35 * (12.0 / 20.0) * (12.0 / 20.0)
        + 0.25 * 0.0 * 0.0),          -- r_factor baseline risk ~ vt_after
    '0x20260708PHX33_45NDrainageDecayBODTSSCEC',
    'cyboquatic_drainagedecay_20260707'
);
