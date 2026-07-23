-- eco_restoration_shard/cyboquatics/2026-07-22-c-drainage_decay/sql/drainagedecay_schema.sql

-- This schema is portable between PostgreSQL and SQLite with minimal adjustments.
-- All constraints are explicit to maintain ecological safety and data integrity.

PRAGMA foreign_keys = ON;

-- Core canal node table with FOG parameters
CREATE TABLE IF NOT EXISTS canal_node (
    canal_node_id        TEXT PRIMARY KEY,
    name                 TEXT NOT NULL,
    latitude_deg         REAL NOT NULL,
    longitude_deg        REAL NOT NULL,
    fog_region_id        TEXT NOT NULL,
    fog_channel_id       TEXT NOT NULL,
    max_frame_energy_j   REAL NOT NULL CHECK (max_frame_energy_j >= 0.0),
    max_bod_mg_l         REAL NOT NULL CHECK (max_bod_mg_l >= 0.0),
    max_tss_mg_l         REAL NOT NULL CHECK (max_tss_mg_l >= 0.0),
    max_cec_cmol_per_kg  REAL NOT NULL CHECK (max_cec_cmol_per_kg >= 0.0),
    created_at_utc       TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS ker_profile (
    ker_profile_id       TEXT PRIMARY KEY,
    canal_node_id        TEXT NOT NULL,
    k_knowledge_factor   REAL NOT NULL CHECK (k_knowledge_factor >= 0.0 AND k_knowledge_factor <= 1.0),
    e_eco_impact         REAL NOT NULL CHECK (e_eco_impact >= 0.0 AND e_eco_impact <= 1.0),
    r_risk_factor        REAL NOT NULL CHECK (r_risk_factor >= 0.0 AND r_risk_factor <= 1.0),
    ker_score            REAL NOT NULL,
    governance_particle_hex TEXT NOT NULL,
    created_at_utc       TEXT NOT NULL,
    FOREIGN KEY (canal_node_id) REFERENCES canal_node(canal_node_id) ON DELETE CASCADE
);

-- Drainage decay frames table with strict invariants
CREATE TABLE IF NOT EXISTS drainagedecay_frame (
    frame_id             TEXT PRIMARY KEY,
    canal_node_id        TEXT NOT NULL,
    ker_profile_id       TEXT NOT NULL,
    timestamp_utc        TEXT NOT NULL,
    bod_mg_l             REAL NOT NULL CHECK (bod_mg_l >= 0.0 AND bod_mg_l <= 80.0),
    tss_mg_l             REAL NOT NULL CHECK (tss_mg_l >= 0.0 AND tss_mg_l <= 500.0),
    cec_cmol_per_kg      REAL NOT NULL CHECK (cec_cmol_per_kg >= 0.0 AND cec_cmol_per_kg <= 100.0),
    frame_energy_j       REAL NOT NULL CHECK (frame_energy_j >= 0.0),
    delta_vt_mps         REAL NOT NULL CHECK (delta_vt_mps >= -5.0 AND delta_vt_mps <= 5.0),
    k_knowledge_factor   REAL NOT NULL CHECK (k_knowledge_factor >= 0.0 AND k_knowledge_factor <= 1.0),
    e_eco_impact         REAL NOT NULL CHECK (e_eco_impact >= 0.0 AND e_eco_impact <= 1.0),
    r_risk_factor        REAL NOT NULL CHECK (r_risk_factor >= 0.0 AND r_risk_factor <= 1.0),
    ker_score            REAL NOT NULL,
    fog_region_id        TEXT NOT NULL,
    fog_channel_id       TEXT NOT NULL,
    governance_particle_hex TEXT NOT NULL,
    FOREIGN KEY (canal_node_id) REFERENCES canal_node(canal_node_id) ON DELETE CASCADE,
    FOREIGN KEY (ker_profile_id) REFERENCES ker_profile(ker_profile_id) ON DELETE CASCADE
);

-- Indexes for energy and eco-impact queries
CREATE INDEX IF NOT EXISTS idx_drainagedecay_canal_node_timestamp
    ON drainagedecay_frame (canal_node_id, timestamp_utc);

CREATE INDEX IF NOT EXISTS idx_drainagedecay_ker_score
    ON drainagedecay_frame (ker_score);

CREATE INDEX IF NOT EXISTS idx_drainagedecay_fog
    ON drainagedecay_frame (fog_region_id, fog_channel_id);

-- Invariant: ker_score must equal k * (e - r)
-- SQLite lacks full CHECK expressions with custom functions, so we emulate this via trigger.

DROP TRIGGER IF EXISTS trg_drainagedecay_ker_invariant;

CREATE TRIGGER trg_drainagedecay_ker_invariant
BEFORE INSERT ON drainagedecay_frame
BEGIN
    -- Reject negative or zero ker_score
    SELECT
        CASE
            WHEN NEW.ker_score <= 0.0 THEN
                RAISE(ABORT, 'ker_score must be positive')
        END;

    -- Basic bound check to avoid obviously invalid values
    SELECT
        CASE
            WHEN NEW.k_knowledge_factor * (NEW.e_eco_impact - NEW.r_risk_factor) - NEW.ker_score > 0.000001
                OR NEW.k_knowledge_factor * (NEW.e_eco_impact - NEW.r_risk_factor) - NEW.ker_score < -0.000001
            THEN
                RAISE(ABORT, 'ker_score inconsistent with KER triad')
        END;
END;
