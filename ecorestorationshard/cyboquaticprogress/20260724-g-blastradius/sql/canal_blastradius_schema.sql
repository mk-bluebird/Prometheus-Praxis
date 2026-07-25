-- file: ecorestorationshard/cyboquaticprogress/20260724-g-blastradius/sql/canal_blastradius_schema.sql
-- destination: ecorestorationshard/cyboquaticprogress/20260724-g-blastradius/sql/canal_blastradius_schema.sql
-- purpose: Canonical SQLite schema for cyboquatic canal blast-radius diagnostics,
--          with KER, FOG, and canal node parameters under strict invariants.
-- non-actuating, carbon-negative oriented, index-ready for real machinery telemetry [file:8][file:4].

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Canal nodes with FOG and energy envelopes
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS canal_node (
    canal_node_id      TEXT PRIMARY KEY,
    name               TEXT NOT NULL,
    latitude_deg       REAL NOT NULL,
    longitude_deg      REAL NOT NULL,
    fog_region_id      TEXT NOT NULL,
    fog_channel_id     TEXT NOT NULL,
    -- Energy envelope for surcharge diagnostic workloads (J per diagnostic window).
    max_diag_energy_j  REAL NOT NULL CHECK (max_diag_energy_j >= 0.0),
    -- Maximum allowable surcharge depth for this node (m).
    max_surcharge_m    REAL NOT NULL CHECK (max_surcharge_m >= 0.0),
    -- Topography proxy (dimensionless 0-1) for surrounding terrain sensitivity.
    topo_sensitivity   REAL NOT NULL CHECK (topo_sensitivity >= 0.0 AND topo_sensitivity <= 1.0),
    created_at_utc     TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_canal_node_fog
    ON canal_node (fog_region_id, fog_channel_id);

----------------------------------------------------------------------
-- 2. KER profiles bound to canal nodes
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS ker_profile (
    ker_profile_id       TEXT PRIMARY KEY,
    canal_node_id        TEXT NOT NULL,
    -- Baseline KER triad for this node (diagnostic corridor context).
    k_knowledge_factor   REAL NOT NULL CHECK (k_knowledge_factor >= 0.0 AND k_knowledge_factor <= 1.0),
    e_eco_impact         REAL NOT NULL CHECK (e_eco_impact       >= 0.0 AND e_eco_impact       <= 1.0),
    r_risk_factor        REAL NOT NULL CHECK (r_risk_factor      >= 0.0 AND r_risk_factor      <= 1.0),
    ker_score            REAL NOT NULL,
    governance_particle_hex TEXT NOT NULL,
    created_at_utc       TEXT NOT NULL,
    FOREIGN KEY (canal_node_id) REFERENCES canal_node(canal_node_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_ker_profile_node
    ON ker_profile (canal_node_id);

----------------------------------------------------------------------
-- 3. Surcharge events (non-actuating evidence of canal stress)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS surcharge_event (
    surcharge_event_id  TEXT PRIMARY KEY,
    canal_node_id       TEXT NOT NULL,
    ts_utc              TEXT NOT NULL,
    -- Observed or modeled surcharge depth at the node (m).
    surcharge_m         REAL NOT NULL CHECK (surcharge_m >= 0.0),
    -- Hydraulic head at time of event (m).
    hydraulic_head_m    REAL NOT NULL CHECK (hydraulic_head_m >= 0.0),
    -- Energy used to simulate/diagnose this event (J).
    diag_energy_j       REAL NOT NULL CHECK (diag_energy_j >= 0.0),
    -- Telemetry quality plane (0-1).
    r_data_quality      REAL NOT NULL CHECK (r_data_quality >= 0.0 AND r_data_quality <= 1.0),
    -- Evidence hex for this event (Phoenix hex registry anchor).
    evidence_hex        TEXT NOT NULL,
    created_at_utc      TEXT NOT NULL,
    FOREIGN KEY (canal_node_id) REFERENCES canal_node(canal_node_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_surcharge_event_node_ts
    ON surcharge_event (canal_node_id, ts_utc);

----------------------------------------------------------------------
-- 4. Blast-radius diagnostics per surcharge event
--    Risk planes + Lyapunov residual + KER scores [file:18][file:31].
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS blast_radius_diag (
    blast_diag_id       TEXT PRIMARY KEY,
    surcharge_event_id  TEXT NOT NULL,
    ker_profile_id      TEXT NOT NULL,
    ts_utc              TEXT NOT NULL,
    -- Computed radial distance of harmful surcharge effects (m).
    radius_m            REAL NOT NULL CHECK (radius_m >= 0.0),
    -- Normalized risk coordinates in [0,1].
    r_hydraulics        REAL NOT NULL CHECK (r_hydraulics >= 0.0        AND r_hydraulics <= 1.0),
    r_energy            REAL NOT NULL CHECK (r_energy      >= 0.0        AND r_energy      <= 1.0),
    r_topology          REAL NOT NULL CHECK (r_topology    >= 0.0        AND r_topology    <= 1.0),
    r_biodiversity      REAL NOT NULL CHECK (r_biodiversity>= 0.0        AND r_biodiversity<= 1.0),
    -- Lyapunov residual scalar Vt (>= 0), using Vt = Σ w_j r_j^2 [file:18][file:31].
    vt_residual         REAL NOT NULL CHECK (vt_residual   >= 0.0),
    -- KER triad and composite score for this diagnostic window.
    k_knowledge_factor  REAL NOT NULL CHECK (k_knowledge_factor >= 0.0 AND k_knowledge_factor <= 1.0),
    e_eco_impact        REAL NOT NULL CHECK (e_eco_impact       >= 0.0 AND e_eco_impact       <= 1.0),
    r_risk_factor       REAL NOT NULL CHECK (r_risk_factor      >= 0.0 AND r_risk_factor      <= 1.0),
    ker_score           REAL NOT NULL,
    -- Derived energy-per-radius metric (J/m) for carbon-negative evaluation.
    energy_per_m_j      REAL NOT NULL CHECK (energy_per_m_j >= 0.0),
    fog_region_id       TEXT NOT NULL,
    fog_channel_id      TEXT NOT NULL,
    governance_particle_hex TEXT NOT NULL,
    created_at_utc      TEXT NOT NULL,
    FOREIGN KEY (surcharge_event_id) REFERENCES surcharge_event(surcharge_event_id) ON DELETE CASCADE,
    FOREIGN KEY (ker_profile_id)     REFERENCES ker_profile(ker_profile_id)           ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_blast_radius_node_time
    ON blast_radius_diag (fog_region_id, fog_channel_id, ts_utc);

CREATE INDEX IF NOT EXISTS idx_blast_radius_vt
    ON blast_radius_diag (vt_residual);

CREATE INDEX IF NOT EXISTS idx_blast_radius_ker
    ON blast_radius_diag (ker_score);

----------------------------------------------------------------------
-- 5. Invariants via triggers:
--    - Enforce KER score consistency: ker_score = k * e - r.
--    - Enforce energy diagnostic envelope: diag_energy_j <= canal_node.max_diag_energy_j.
----------------------------------------------------------------------

DROP TRIGGER IF EXISTS trg_ker_profile_invariant;
DROP TRIGGER IF EXISTS trg_blast_radius_ker_invariant;
DROP TRIGGER IF EXISTS trg_surcharge_energy_envelope;

-- KER invariant at ker_profile level.
CREATE TRIGGER trg_ker_profile_invariant
BEFORE INSERT ON ker_profile
BEGIN
    -- Positive ker_score only; disallow non-positive KER for governance baselines.
    SELECT CASE
        WHEN NEW.ker_score <= 0.0 THEN
            RAISE(ABORT, 'ker_profile.ker_score must be > 0.0')
    END;
    -- Coarse consistency check k*e - r ≈ ker_score.
    SELECT CASE
        WHEN (NEW.k_knowledge_factor * NEW.e_eco_impact - NEW.r_risk_factor) - NEW.ker_score > 0.000001
             OR (NEW.k_knowledge_factor * NEW.e_eco_impact - NEW.r_risk_factor) - NEW.ker_score < -0.000001
        THEN
            RAISE(ABORT, 'ker_profile KER triad inconsistent with ker_score')
    END;
END;

-- KER invariant for blast-radius diagnostics.
CREATE TRIGGER trg_blast_radius_ker_invariant
BEFORE INSERT ON blast_radius_diag
BEGIN
    -- Disallow non-positive ker_score for diagnostics that might gate automation.
    SELECT CASE
        WHEN NEW.ker_score <= 0.0 THEN
            RAISE(ABORT, 'blast_radius_diag.ker_score must be > 0.0')
    END;
    -- Coarse consistency check for diagnostic KER triad.
    SELECT CASE
        WHEN (NEW.k_knowledge_factor * NEW.e_eco_impact - NEW.r_risk_factor) - NEW.ker_score > 0.000001
             OR (NEW.k_knowledge_factor * NEW.e_eco_impact - NEW.r_risk_factor) - NEW.ker_score < -0.000001
        THEN
            RAISE(ABORT, 'blast_radius_diag KER triad inconsistent with ker_score')
    END;
END;

-- Energy envelope invariant: surcharge diagnostics must respect node-level max_diag_energy_j.
CREATE TRIGGER trg_surcharge_energy_envelope
BEFORE INSERT ON surcharge_event
BEGIN
    SELECT CASE
        WHEN NEW.diag_energy_j >
             (SELECT max_diag_energy_j FROM canal_node WHERE canal_node_id = NEW.canal_node_id)
        THEN
            RAISE(ABORT, 'surcharge_event.diag_energy_j exceeds canal_node.max_diag_energy_j')
    END;
END;
