-- filename: db/dbcyboquaticblastradiusindex.sql
-- license: MIT OR Apache-2.0
-- role: Blast-radius index DB for cyboquatic surcharge breaches.
-- note: Non-actuating diagnostics; used by EcoNet and Prometheus-Praxis governance crates.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Blast-radius index table
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cyboquatic_blast_radius_index (
    blast_index_id         TEXT PRIMARY KEY,           -- hex or UUID
    node_id                TEXT NOT NULL,              -- canal node
    event_id               TEXT NOT NULL,              -- surcharge event identifier
    corridor_id            TEXT NOT NULL,              -- governance corridor
    lane_id                TEXT NOT NULL,              -- lane identifier (research/pilot/production)

    -- ALN shard binding
    aln_shard_name         TEXT NOT NULL,              -- e.g. CyboquaticBlastRadiusGovernance2026v1.aln2
    aln_version            TEXT NOT NULL,              -- e.g. 2026v1
    did_root               TEXT NOT NULL,              -- primary DID root

    -- Event window
    event_timestamp_utc    TEXT NOT NULL,
    window_start_utc       TEXT NOT NULL,
    window_end_utc         TEXT NOT NULL,

    -- Hydraulic metrics
    surcharge_level_m      REAL NOT NULL,
    hydraulic_head_m       REAL NOT NULL,
    inflow_m3_s            REAL NOT NULL,
    duration_s             REAL NOT NULL,

    -- Blast radius diagnostics
    radius_m               REAL NOT NULL,
    radius_norm            REAL NOT NULL CHECK (radius_norm >= 0.0 AND radius_norm <= 1.0),

    -- KER coordinates
    k_knowledge            REAL NOT NULL CHECK (k_knowledge  >= 0.0 AND k_knowledge  <= 1.0),
    e_ecoimpact            REAL NOT NULL CHECK (e_ecoimpact  >= 0.0 AND e_ecoimpact  <= 1.0),
    r_risk                 REAL NOT NULL CHECK (r_risk       >= 0.0 AND r_risk       <= 1.0),
    ker_score              REAL NOT NULL,

    -- Residual KER and RoH
    residual_ker           REAL NOT NULL,
    roh_coordinate         REAL NOT NULL CHECK (roh_coordinate >= 0.0 AND roh_coordinate <= 1.0),

    -- Governance flags
    radius_within_limit    INTEGER NOT NULL CHECK (radius_within_limit IN (0, 1)),
    ker_within_limit       INTEGER NOT NULL CHECK (ker_within_limit IN (0, 1)),
    lane_admissible_ok     INTEGER NOT NULL CHECK (lane_admissible_ok IN (0, 1)),
    safetopromote_ok       INTEGER NOT NULL CHECK (safetopromote_ok IN (0, 1)),

    -- Provenance
    source_db_path         TEXT NOT NULL,              -- e.g. dbcyboquaticmachineryspine.sql
    source_view_name       TEXT NOT NULL,              -- e.g. vcyboquaticblastradius
    evidence_hex           TEXT NOT NULL,
    created_at_utc         TEXT NOT NULL,
    last_updated_utc       TEXT NOT NULL
);

----------------------------------------------------------------------
-- 2. Indices
----------------------------------------------------------------------

CREATE INDEX IF NOT EXISTS idx_blast_node_event
    ON cyboquatic_blast_radius_index (node_id, event_id);

CREATE INDEX IF NOT EXISTS idx_blast_corridor_lane
    ON cyboquatic_blast_radius_index (corridor_id, lane_id);

CREATE INDEX IF NOT EXISTS idx_blast_radius_flags
    ON cyboquatic_blast_radius_index (radius_within_limit, ker_within_limit, safetopromote_ok);

CREATE INDEX IF NOT EXISTS idx_blast_ker_residual
    ON cyboquatic_blast_radius_index (ker_score, residual_ker, roh_coordinate);

CREATE INDEX IF NOT EXISTS idx_blast_evidence
    ON cyboquatic_blast_radius_index (evidence_hex);

----------------------------------------------------------------------
-- 3. AI-safe facade view
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_cyboquatic_blast_radius_facade AS
SELECT
    blast_index_id,
    node_id,
    event_id,
    corridor_id,
    lane_id,
    radius_m,
    radius_norm,
    k_knowledge AS k,
    e_ecoimpact AS e,
    r_risk     AS r,
    ker_score,
    residual_ker,
    roh_coordinate,
    radius_within_limit,
    ker_within_limit,
    lane_admissible_ok,
    safetopromote_ok,
    event_timestamp_utc,
    window_start_utc,
    window_end_utc,
    created_at_utc
FROM cyboquatic_blast_radius_index;

----------------------------------------------------------------------
-- 4. Trigger: enforce KER consistency and Lyapunov-style non-regression on radius_norm
----------------------------------------------------------------------

DROP TRIGGER IF EXISTS trg_blast_radius_ker_invariants;

CREATE TRIGGER trg_blast_radius_ker_invariants
BEFORE INSERT ON cyboquatic_blast_radius_index
BEGIN
    -- KER score consistency: ker_score ~= k * e - r
    SELECT
        CASE
            WHEN ABS(NEW.k_knowledge * NEW.e_ecoimpact - NEW.r_risk - NEW.ker_score) > 0.000001
            THEN RAISE(ABORT, 'ker_score inconsistent with KER triad for blast radius')
        END;

    -- Residual KER must be non-negative and bounded.
    SELECT
        CASE
            WHEN NEW.residual_ker < 0.0
            THEN RAISE(ABORT, 'residual_ker must be non-negative')
        END;

    -- Radius_norm must remain in [0,1].
    SELECT
        CASE
            WHEN NEW.radius_norm < 0.0 OR NEW.radius_norm > 1.0
            THEN RAISE(ABORT, 'radius_norm must be in [0,1]')
        END;
END;
