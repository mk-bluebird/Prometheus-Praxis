-- File: sql/ker/module_ker_profile.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/ker/module_ker_profile.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS module_ker_profile (
    module_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_name        TEXT NOT NULL,
    relpath          TEXT NOT NULL,  -- e.g. "cpp/tools/eco_synapse_cpp_bridge.cpp"
    did_owner        TEXT NOT NULL,  -- DID of primary owner
    steward_did      TEXT NULL,      -- optional steward/reviewer DID
    lane_default     TEXT NOT NULL CHECK (lane_default IN ('RESEARCH', 'EXPPROD', 'PROD')),
    primary_plane    TEXT NOT NULL,  -- e.g. "CARBON", "HYDRAULICS", "NEURO"
    module_role      TEXT NOT NULL CHECK (module_role IN ('ANALYTIC', 'ACTUATION_GATE', 'PURE_GOVERNANCE')),

    ker_k            REAL NOT NULL DEFAULT 0.0,
    ker_e            REAL NOT NULL DEFAULT 0.0,
    ker_r            REAL NOT NULL DEFAULT 0.0,
    ker_s            REAL NOT NULL DEFAULT 0.0,

    non_actuating    INTEGER NOT NULL DEFAULT 1 CHECK (non_actuating IN (0, 1)),
    neuro_flag       INTEGER NOT NULL DEFAULT 0 CHECK (neuro_flag IN (0, 1)),
    citizen_ready    INTEGER NOT NULL DEFAULT 0 CHECK (citizen_ready IN (0, 1)),

    created_utc      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE INDEX IF NOT EXISTS idx_module_ker_relpath
    ON module_ker_profile (repo_name, relpath);

CREATE INDEX IF NOT EXISTS idx_module_ker_lane
    ON module_ker_profile (lane_default, primary_plane);

-- KER bounds and scalar consistency, mirrored from ALN KerBounds + KerScalarConsistency.

DROP TRIGGER IF EXISTS trg_module_ker_bounds_insert;

CREATE TRIGGER trg_module_ker_bounds_insert
BEFORE INSERT ON module_ker_profile
BEGIN
    SELECT CASE
        WHEN NEW.ker_k < 0.0 OR NEW.ker_k > 1.0 THEN
            RAISE(ABORT, 'module_ker_profile.ker_k must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_e < 0.0 OR NEW.ker_e > 1.0 THEN
            RAISE(ABORT, 'module_ker_profile.ker_e must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_r < 0.0 OR NEW.ker_r > 1.0 THEN
            RAISE(ABORT, 'module_ker_profile.ker_r must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_s != NEW.ker_k * NEW.ker_e - NEW.ker_r THEN
            RAISE(ABORT, 'module_ker_profile.ker_s must equal ker_k * ker_e - ker_r')
    END;
END;

DROP TRIGGER IF EXISTS trg_module_ker_bounds_update;

CREATE TRIGGER trg_module_ker_bounds_update
BEFORE UPDATE ON module_ker_profile
BEGIN
    SELECT CASE
        WHEN NEW.ker_k < 0.0 OR NEW.ker_k > 1.0 THEN
            RAISE(ABORT, 'module_ker_profile.ker_k must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_e < 0.0 OR NEW.ker_e > 1.0 THEN
            RAISE(ABORT, 'module_ker_profile.ker_e must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_r < 0.0 OR NEW.ker_r > 1.0 THEN
            RAISE(ABORT, 'module_ker_profile.ker_r must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_s != NEW.ker_k * NEW.ker_e - NEW.ker_r THEN
            RAISE(ABORT, 'module_ker_profile.ker_s must equal ker_k * ker_e - ker_r')
    END;
END;

-- KerPositiveForNonResearch: RESEARCH lane can tolerate ker_s <= 0,
-- non-RESEARCH lanes require ker_s > 0.

DROP TRIGGER IF EXISTS trg_module_ker_positive_nonresearch;

CREATE TRIGGER trg_module_ker_positive_nonresearch
BEFORE INSERT ON module_ker_profile
BEGIN
    SELECT CASE
        WHEN NEW.lane_default != 'RESEARCH' AND NEW.ker_s <= 0.0 THEN
            RAISE(ABORT, 'Non-RESEARCH modules require ker_s > 0.0')
    END;
END;

DROP TRIGGER IF EXISTS trg_module_ker_positive_nonresearch_update;

CREATE TRIGGER trg_module_ker_positive_nonresearch_update
BEFORE UPDATE ON module_ker_profile
BEGIN
    SELECT CASE
        WHEN NEW.lane_default != 'RESEARCH' AND NEW.ker_s <= 0.0 THEN
            RAISE(ABORT, 'Non-RESEARCH modules require ker_s > 0.0')
    END;
END;

-- NonActuatingRoleConsistency: non_actuating=true => module_role is ANALYTIC or PURE_GOVERNANCE.

DROP TRIGGER IF EXISTS trg_module_nonactuating_role;

CREATE TRIGGER trg_module_nonactuating_role
BEFORE INSERT ON module_ker_profile
BEGIN
    SELECT CASE
        WHEN NEW.non_actuating = 1
             AND NEW.module_role NOT IN ('ANALYTIC', 'PURE_GOVERNANCE') THEN
            RAISE(ABORT, 'Non-actuating modules must be ANALYTIC or PURE_GOVERNANCE')
    END;
END;

DROP TRIGGER IF EXISTS trg_module_nonactuating_role_update;

CREATE TRIGGER trg_module_nonactuating_role_update
BEFORE UPDATE ON module_ker_profile
BEGIN
    SELECT CASE
        WHEN NEW.non_actuating = 1
             AND NEW.module_role NOT IN ('ANALYTIC', 'PURE_GOVERNANCE') THEN
            RAISE(ABORT, 'Non-actuating modules must be ANALYTIC or PURE_GOVERNANCE')
    END;
END;

-- ProdLaneGovernance: PROD lane requires stronger KER thresholds and citizen_ready=true.

DROP TRIGGER IF EXISTS trg_module_prod_lane;

CREATE TRIGGER trg_module_prod_lane
BEFORE INSERT ON module_ker_profile
BEGIN
    SELECT CASE
        WHEN NEW.lane_default = 'PROD' AND NEW.ker_s <= 0.2 THEN
            RAISE(ABORT, 'PROD modules must have ker_s > 0.2')
    END;
    SELECT CASE
        WHEN NEW.lane_default = 'PROD' AND NEW.ker_e < 0.7 THEN
            RAISE(ABORT, 'PROD modules must have ker_e >= 0.7')
    END;
    SELECT CASE
        WHEN NEW.lane_default = 'PROD' AND NEW.ker_r > 0.5 THEN
            RAISE(ABORT, 'PROD modules must have ker_r <= 0.5')
    END;
    SELECT CASE
        WHEN NEW.lane_default = 'PROD' AND NEW.citizen_ready = 0 THEN
            RAISE(ABORT, 'PROD modules must be citizen_ready = 1')
    END;
END;

DROP TRIGGER IF EXISTS trg_module_prod_lane_update;

CREATE TRIGGER trg_module_prod_lane_update
BEFORE UPDATE ON module_ker_profile
BEGIN
    SELECT CASE
        WHEN NEW.lane_default = 'PROD' AND NEW.ker_s <= 0.2 THEN
            RAISE(ABORT, 'PROD modules must have ker_s > 0.2')
    END;
    SELECT CASE
        WHEN NEW.lane_default = 'PROD' AND NEW.ker_e < 0.7 THEN
            RAISE(ABORT, 'PROD modules must have ker_e >= 0.7')
    END;
    SELECT CASE
        WHEN NEW.lane_default = 'PROD' AND NEW.ker_r > 0.5 THEN
            RAISE(ABORT, 'PROD modules must have ker_r <= 0.5')
    END;
    SELECT CASE
        WHEN NEW.lane_default = 'PROD' AND NEW.citizen_ready = 0 THEN
            RAISE(ABORT, 'PROD modules must be citizen_ready = 1')
    END;
END;

-- HighRiskGovernance: if ker_r >= 0.7, module must be neuro_flag=1, citizen_ready=false, lane_default='RESEARCH'.

DROP TRIGGER IF EXISTS trg_module_high_risk_governance;

CREATE TRIGGER trg_module_high_risk_governance
BEFORE INSERT ON module_ker_profile
BEGIN
    SELECT CASE
        WHEN NEW.ker_r >= 0.7
             AND NOT (NEW.neuro_flag = 1 AND NEW.citizen_ready = 0 AND NEW.lane_default = 'RESEARCH') THEN
            RAISE(ABORT, 'High-risk modules require neuro_flag=1, citizen_ready=0, lane_default=RESEARCH')
    END;
END;

DROP TRIGGER IF EXISTS trg_module_high_risk_governance_update;

CREATE TRIGGER trg_module_high_risk_governance_update
BEFORE UPDATE ON module_ker_profile
BEGIN
    SELECT CASE
        WHEN NEW.ker_r >= 0.7
             AND NOT (NEW.neuro_flag = 1 AND NEW.citizen_ready = 0 AND NEW.lane_default = 'RESEARCH') THEN
            RAISE(ABORT, 'High-risk modules require neuro_flag=1, citizen_ready=0, lane_default=RESEARCH')
    END;
END;

-- Auto-update updated_utc on write.

DROP TRIGGER IF EXISTS trg_module_ker_profile_timestamp;

CREATE TRIGGER trg_module_ker_profile_timestamp
AFTER UPDATE ON module_ker_profile
BEGIN
    UPDATE module_ker_profile
    SET updated_utc = (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
    WHERE module_id = NEW.module_id;
END;
