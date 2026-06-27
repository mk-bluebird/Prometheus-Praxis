-- filename: db_blast_radius_kernel.sql
-- destination: ecorestoration_shard/db/db_blast_radius_kernel.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Substrate kinetics metadata
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS substrate_kinetics (
    kinetics_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    substrate_kind     TEXT NOT NULL,           -- e.g. FLOWVAC, CONCRETE, BIOPOLYMER
    contaminant_code   TEXT NOT NULL,           -- e.g. PFBS, PFOS, NANOBYPRODUCT
    k_base_per_day     REAL NOT NULL,           -- base first-order decay constant [1/day]
    temp_ref_celsius   REAL NOT NULL,           -- reference temperature for k_base
    season_code        TEXT NOT NULL,           -- e.g. SUMMER, WINTER, MONSOON
    evidence_hex       TEXT NOT NULL,           -- hex stamp for provenance
    created_utc        TEXT NOT NULL,
    updated_utc        TEXT NOT NULL,
    UNIQUE (substrate_kind, contaminant_code, season_code)
);

----------------------------------------------------------------------
-- 2. Segment-level velocity snapshots
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS segment_velocity_snapshot (
    snapshot_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    segment_id    INTEGER NOT NULL,      -- FK into cyboasset.assetid or a segment catalog
    region_code   TEXT NOT NULL,         -- e.g. PHX-CANAL-SEG-07
    v_mean_m_per_s REAL NOT NULL,       -- mean local transport velocity [m/s]
    v_std_m_per_s  REAL NOT NULL,       -- std dev for telemetry confidence
    telemetry_span_s INTEGER NOT NULL,  -- duration of telemetry window [s]
    t_start_utc   TEXT NOT NULL,
    t_end_utc     TEXT NOT NULL,
    created_utc   TEXT NOT NULL,
    updated_utc   TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_segment_velocity_segment
    ON segment_velocity_snapshot (segment_id, t_end_utc);

CREATE INDEX IF NOT EXISTS idx_segment_velocity_region
    ON segment_velocity_snapshot (region_code, t_end_utc);

----------------------------------------------------------------------
-- 3. Eco-weight configuration for blast-radius
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS blast_radius_eco_weight (
    weight_id              INTEGER PRIMARY KEY AUTOINCREMENT,
    segment_id             INTEGER NOT NULL,
    region_code            TEXT NOT NULL,
    non_offsettable_carbon INTEGER NOT NULL,  -- 0 or 1
    non_offsettable_bio    INTEGER NOT NULL,  -- 0 or 1
    restoration_bonus      REAL NOT NULL,     -- >= 0, e.g. 0.0 .. 1.0
    eco_weight_min         REAL NOT NULL,     -- lower bound, >= 1.0
    eco_weight_max         REAL NOT NULL,     -- upper bound, >= eco_weight_min
    created_utc            TEXT NOT NULL,
    updated_utc            TEXT NOT NULL,
    UNIQUE (segment_id, region_code)
);

----------------------------------------------------------------------
-- 4. Blast-radius lambda cache per segment
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS blast_radius_lambda_cache (
    cache_id               INTEGER PRIMARY KEY AUTOINCREMENT,
    segment_id             INTEGER NOT NULL,
    region_code            TEXT NOT NULL,
    contaminant_code       TEXT NOT NULL,
    substrate_kind         TEXT NOT NULL,
    k_eff_per_day          REAL NOT NULL,      -- effective decay constant [1/day]
    v_mean_m_per_s         REAL NOT NULL,      -- mean velocity used for lambda [m/s]
    eco_weight_applied     REAL NOT NULL,      -- final eco weight factor
    lambda_eff_per_m       REAL NOT NULL,      -- effective spatial decay [1/m]
    lambda_eff_min_per_m   REAL NOT NULL,      -- lower bound for lambda [1/m]
    lambda_eff_max_per_m   REAL NOT NULL,      -- upper bound for lambda [1/m]
    telemetry_span_s       INTEGER NOT NULL,   -- underlying telemetry integration span [s]
    t_snapshot_start_utc   TEXT NOT NULL,
    t_snapshot_end_utc     TEXT NOT NULL,
    created_utc            TEXT NOT NULL,
    updated_utc            TEXT NOT NULL,
    UNIQUE (segment_id, region_code, contaminant_code, t_snapshot_end_utc)
);

CREATE INDEX IF NOT EXISTS idx_lambda_cache_segment
    ON blast_radius_lambda_cache (segment_id, t_snapshot_end_utc);

CREATE INDEX IF NOT EXISTS idx_lambda_cache_region
    ON blast_radius_lambda_cache (region_code, t_snapshot_end_utc);

----------------------------------------------------------------------
-- 5. Link to cyboblastradiusbridge (if present in same DB)
--    This table is assumed to exist; we only add lambda fields via a view.
----------------------------------------------------------------------

-- Optional view to expose lambda alongside bridge metadata.
-- It assumes cyboblastradiusbridge(assetid, shardid, linkimpacttype, linkimpactscore, linkvtsensitivity, created_utc) exists.

CREATE VIEW IF NOT EXISTS v_blast_radius_lambda_bridge AS
SELECT
    b.bridgeid             AS bridge_id,
    b.assetid              AS asset_id,
    b.shardid              AS shard_id,
    b.linkimpacttype       AS impact_type,
    b.linkimpactscore      AS impact_score,
    b.linkvtsensitivity    AS vt_sensitivity,
    c.segment_id           AS segment_id,
    c.region_code          AS region_code,
    c.contaminant_code     AS contaminant_code,
    c.lambda_eff_per_m     AS lambda_eff_per_m,
    c.lambda_eff_min_per_m AS lambda_eff_min_per_m,
    c.lambda_eff_max_per_m AS lambda_eff_max_per_m,
    c.t_snapshot_end_utc   AS lambda_snapshot_utc,
    c.created_utc          AS lambda_created_utc
FROM blast_radius_lambda_cache AS c
JOIN cyboblastradiusbridge AS b
    ON b.assetid = c.segment_id AND b.region_code = c.region_code;
