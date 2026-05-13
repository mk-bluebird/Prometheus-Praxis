-- filename: sql/eco_response_shard_schema.sql
-- Purpose: Core tables for eco_response_shard, aligned to
--          ResponseShardEcoMetrics2026v1 and ResponseShardCalibSigmaInvariants2026v1.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Core response_shard table
--    One row per summarized shardinstance, used for governance proofs.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS response_shard (
    response_id      INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Identity / linkage back to econet-index
    shard_id         INTEGER NOT NULL,          -- econet-index.shardinstance.shardid
    topic_tag        TEXT    NOT NULL,          -- lane/medium/region composite
    user_did         TEXT    NOT NULL,          -- Bostrom DID for authorship
    node_id          TEXT,                      -- optional node identifier
    asset_type       TEXT,
    medium           TEXT,
    region           TEXT,
    lane             TEXT    NOT NULL,          -- RESEARCH, EXPPROD, PROD

    -- Raw KER windows from shardinstance
    k_metric         REAL    NOT NULL,
    e_metric         REAL    NOT NULL,
    r_metric         REAL    NOT NULL,
    vt_max           REAL    NOT NULL,

    -- Meta-KER factors from knowledgeecoscore
    k_factor         REAL    NOT NULL,
    e_factor         REAL    NOT NULL,
    r_factor         REAL    NOT NULL,

    -- Scalar residual in 0..1 (normalized vt_max + other non-negative planes)
    residual_score   REAL    NOT NULL,

    -- Uncertainty / ingest linkage (populated in phase 2, optional at first)
    ingest_window_id INTEGER,                   -- FK to ingest diagnostics table (external DB or future local table)
    rcalib           REAL,                      -- 0..1, data quality risk
    rsigma           REAL,                      -- 0..1, uncertainty risk
    d_data           REAL,                      -- Ddata = 1 - rcalib
    d_combined       REAL,                      -- Dcombined = Dsensor * Ddata
    k_adj            REAL,                      -- adjusted K after uncertainty penalties
    e_adj            REAL,                      -- adjusted E after uncertainty penalties,

    -- Provenance
    evidencehex      TEXT,                      -- hex hash of row payload
    signinghex       TEXT,                      -- DID-backed signature
    parentevidencehex TEXT,                     -- hash chain back to upstream shard

    created_utc      TEXT    NOT NULL           -- ISO8601 insertion timestamp
);

-- Core indexes for typical governance queries
CREATE INDEX IF NOT EXISTS idx_response_shard_lane_ker
    ON response_shard (lane, k_metric, e_metric, r_metric);

CREATE INDEX IF NOT EXISTS idx_response_shard_region_medium
    ON response_shard (region, medium);

CREATE INDEX IF NOT EXISTS idx_response_shard_topic
    ON response_shard (topic_tag);

CREATE INDEX IF NOT EXISTS idx_response_shard_shardid
    ON response_shard (shard_id);

CREATE INDEX IF NOT EXISTS idx_response_shard_ingest
    ON response_shard (ingest_window_id);

CREATE INDEX IF NOT EXISTS idx_response_shard_residual
    ON response_shard (residual_score);

-- Optional: quick filter on uncertainty planes for CI gating
CREATE INDEX IF NOT EXISTS idx_response_shard_uncertainty
    ON response_shard (lane, rcalib, rsigma, d_combined);


----------------------------------------------------------------------
-- 2. response_calib_sigma table
--    Diagnostic invariant layer for rcalib / rsigma and vt_before/after.
--    Matches ResponseShardCalibSigmaInvariants2026v1.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS response_calib_sigma (
    response_id     INTEGER PRIMARY KEY
                    REFERENCES response_shard(response_id)
                    ON DELETE CASCADE,

    shard_id        INTEGER NOT NULL,        -- duplicate of response_shard.shard_id for convenience
    rcalib          REAL    NOT NULL,        -- 0..1
    rsigma          REAL    NOT NULL,        -- 0..1

    vt_before       REAL    NOT NULL,        -- residual before adding rcalib/rsigma
    vt_after        REAL    NOT NULL,        -- residual after adding rcalib/rsigma planes

    k_score         REAL    NOT NULL,        -- K after applying data-quality penalties
    e_score         REAL    NOT NULL,        -- E after applying data-quality penalties
    r_score         REAL    NOT NULL,        -- R after applying data-quality penalties

    deploy_decision TEXT    NOT NULL,        -- Deploy, ResearchOnly, BlockedByCalib, BlockedByRisk, BlockedByKER
    lane            TEXT    NOT NULL,        -- copy from response_shard.lane for faster checks

    evidence_hex    TEXT,                    -- provenance for invariant evaluation
    signing_hex     TEXT                     -- signature for invariant evaluation
);

CREATE INDEX IF NOT EXISTS idx_response_calib_sigma_lane
    ON response_calib_sigma (lane, rcalib, rsigma);

CREATE INDEX IF NOT EXISTS idx_response_calib_sigma_decision
    ON response_calib_sigma (deploy_decision);

CREATE INDEX IF NOT EXISTS idx_response_calib_sigma_shard
    ON response_calib_sigma (shard_id);


----------------------------------------------------------------------
-- 3. Optional: backfill meta for provenance of the index itself.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS response_backfill_meta (
    meta_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    econet_db_path TEXT    NOT NULL,      -- source econet-index DB
    snapshot_utc   TEXT    NOT NULL,      -- when the snapshot was taken
    comment        TEXT
);
