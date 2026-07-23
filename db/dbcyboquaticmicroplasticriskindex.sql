-- filename: dbcyboquaticmicroplasticriskindex.sql
-- destination: github.com/mk-bluebird/Prometheus-Praxis/workspace/db/dbcyboquaticmicroplasticriskindex.sql
-- license: MIT OR Apache-2.0

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Canonical microplastic risk index table
--    Bridges ALN shard, machinery index, ecosafety windows, and lane status.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cyboquatic_microplastic_risk_index (
    risk_index_id            TEXT PRIMARY KEY,
    -- Core identity
    node_id                  TEXT NOT NULL,
    shard_id                 TEXT NOT NULL,
    corridor_id              TEXT NOT NULL,
    lane_id                  TEXT NOT NULL,

    -- ALN binding
    aln_shard_name           TEXT NOT NULL,  -- e.g. 'CyboquaticMicroplasticMaterialsRisk2026v1.aln'
    aln_version              TEXT NOT NULL,  -- e.g. '2026v1'
    did_root                 TEXT NOT NULL,  -- e.g. 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'

    -- Raw metrics (per window / node)
    window_start_utc         TEXT NOT NULL,
    window_end_utc           TEXT NOT NULL,
    microplastic_load_mg_m3  REAL NOT NULL,  -- volumetric microplastic concentration
    flow_rate_m3_s           REAL NOT NULL,  -- hydraulic throughput for normalization
    eco_per_joule            REAL NOT NULL,  -- from vcyboquaticecoperjoule
    restoration_score        REAL NOT NULL,  -- from vcyboquaticrestore

    -- KER coordinates (0..1, governed by ALN)
    k_knowledge_factor       REAL NOT NULL CHECK k_knowledge_factor >= 0.0 AND k_knowledge_factor <= 1.0,
    e_eco_impact             REAL NOT NULL CHECK e_eco_impact >= 0.0 AND e_eco_impact <= 1.0,
    r_risk_factor            REAL NOT NULL CHECK r_risk_factor >= 0.0 AND r_risk_factor <= 1.0,
    ker_score                REAL NOT NULL,

    -- Lyapunov and RoH (risk-of-harm)
    vt_before                REAL NOT NULL,
    vt_after                 REAL NOT NULL,
    roh_coordinate           REAL NOT NULL,

    -- Governance flags (computed from eco / restoration / KER)
    carbon_negative_ok       INTEGER NOT NULL CHECK carbon_negative_ok IN (0, 1),
    restoration_ok           INTEGER NOT NULL CHECK restoration_ok IN (0, 1),
    lane_admissible_ok       INTEGER NOT NULL CHECK lane_admissible_ok IN (0, 1),
    safetopromote_ok         INTEGER NOT NULL CHECK safetopromote_ok IN (0, 1),

    -- Source provenance
    source_view_ecoperjoule  TEXT NOT NULL,  -- e.g. 'vcyboquaticecoperjoule'
    source_view_restore      TEXT NOT NULL,  -- e.g. 'vcyboquaticrestore'
    source_db_path           TEXT NOT NULL,  -- e.g. 'db/econetcyboquaticindex2026v1.sql'
    created_at_utc           TEXT NOT NULL,
    last_updated_utc         TEXT NOT NULL
);

----------------------------------------------------------------------
-- 2. Indices for fast governance queries
----------------------------------------------------------------------

CREATE INDEX IF NOT EXISTS idx_microplastic_node_lane
    ON cyboquatic_microplastic_risk_index (node_id, lane_id);

CREATE INDEX IF NOT EXISTS idx_microplastic_corridor_ker
    ON cyboquatic_microplastic_risk_index (corridor_id, ker_score);

CREATE INDEX IF NOT EXISTS idx_microplastic_flags
    ON cyboquatic_microplastic_risk_index (
        carbon_negative_ok,
        restoration_ok,
        lane_admissible_ok,
        safetopromote_ok
    );

CREATE INDEX IF NOT EXISTS idx_microplastic_window_time
    ON cyboquatic_microplastic_risk_index (window_start_utc, window_end_utc);

----------------------------------------------------------------------
-- 3. View: AI-safe facade over microplastic risk index
--    Exposes only non-actuating, corridor-safe fields for AI agents.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vcyboquatic_microplastic_risk_facade AS
SELECT
    risk_index_id,
    node_id,
    shard_id,
    corridor_id,
    lane_id,
    microplastic_load_mg_m3,
    flow_rate_m3_s,
    eco_per_joule,
    restoration_score,
    k_knowledge_factor AS k,
    e_eco_impact       AS e,
    r_risk_factor      AS r,
    ker_score,
    vt_before,
    vt_after,
    roh_coordinate,
    carbon_negative_ok,
    restoration_ok,
    lane_admissible_ok,
    safetopromote_ok,
    created_at_utc,
    last_updated_utc
FROM cyboquatic_microplastic_risk_index;

----------------------------------------------------------------------
-- 4. Invariant trigger: KER consistency and safety corridor
--    Ensures ker_score and basic microplastic corridors remain valid.
----------------------------------------------------------------------

DROP TRIGGER IF EXISTS trg_microplastic_ker_invariants;

CREATE TRIGGER trg_microplastic_ker_invariants
BEFORE INSERT ON cyboquatic_microplastic_risk_index
BEGIN
    -- KER score must be consistent with k * e - r (simple corridor check).
    SELECT
        CASE
            WHEN (
                (NEW.k_knowledge_factor * NEW.e_eco_impact - NEW.r_risk_factor) - NEW.ker_score
            ) > 0.000001
            OR (
                (NEW.k_knowledge_factor * NEW.e_eco_impact - NEW.r_risk_factor) - NEW.ker_score
            ) < -0.000001
            THEN RAISE(ABORT, 'ker_score inconsistent with KER triad for microplastic risk')
        END;

    -- Microplastic load must be non-negative and bounded to avoid obvious harm.
    SELECT
        CASE
            WHEN NEW.microplastic_load_mg_m3 < 0.0
            THEN RAISE(ABORT, 'microplastic_load_mg_m3 must be non-negative')
        END;

    -- Simple Lyapunov non-increase constraint at the window level.
    SELECT
        CASE
            WHEN NEW.vt_after > NEW.vt_before
            THEN RAISE(ABORT, 'Lyapunov residual must be non-increasing (vt_after <= vt_before)')
        END;
END;
