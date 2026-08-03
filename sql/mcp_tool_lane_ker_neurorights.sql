-- File: sql/mcp_tool_lane_ker_neurorights.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/mcp_tool_lane_ker_neurorights.sql

PRAGMA foreign_keys = ON;

-- Lane and planes are likely already present; we add KER and flags.
ALTER TABLE mcp_tool ADD COLUMN ker_k          REAL NOT NULL DEFAULT 0.0;
ALTER TABLE mcp_tool ADD COLUMN ker_e          REAL NOT NULL DEFAULT 0.0;
ALTER TABLE mcp_tool ADD COLUMN ker_r          REAL NOT NULL DEFAULT 0.0;
ALTER TABLE mcp_tool ADD COLUMN ker_s          REAL NOT NULL DEFAULT 0.0;

ALTER TABLE mcp_tool ADD COLUMN neuroflag      INTEGER NOT NULL DEFAULT 0 CHECK (neuroflag IN (0, 1));
ALTER TABLE mcp_tool ADD COLUMN nonactuatingonly INTEGER NOT NULL DEFAULT 1 CHECK (nonactuatingonly IN (0, 1));
ALTER TABLE mcp_tool ADD COLUMN citizen_ready  INTEGER NOT NULL DEFAULT 0 CHECK (citizen_ready IN (0, 1));
ALTER TABLE mcp_tool ADD COLUMN consent_level  TEXT NOT NULL DEFAULT 'NONE'
    CHECK (consent_level IN ('NONE', 'IMPLICIT', 'EXPLICIT'));

ALTER TABLE mcp_tool ADD COLUMN synapse_class  TEXT NOT NULL DEFAULT 'ANALYTIC_BRIDGE'
    CHECK (synapse_class IN ('ANALYTIC_BRIDGE', 'ACTUATION_GATE', 'PURE_GOVERNANCE'));

ALTER TABLE mcp_tool ADD COLUMN allows_readonly INTEGER NOT NULL DEFAULT 1 CHECK (allows_readonly IN (0, 1));
ALTER TABLE mcp_tool ADD COLUMN allows_actuation INTEGER NOT NULL DEFAULT 0 CHECK (allows_actuation IN (0, 1));

-- KER invariants.
DROP TRIGGER IF EXISTS trg_mcp_tool_ker_invariants;

CREATE TRIGGER trg_mcp_tool_ker_invariants
BEFORE INSERT ON mcp_tool
BEGIN
    SELECT CASE
        WHEN NEW.ker_k < 0.0 OR NEW.ker_k > 1.0 THEN
            RAISE(ABORT, 'mcp_tool.ker_k must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_e < 0.0 OR NEW.ker_e > 1.0 THEN
            RAISE(ABORT, 'mcp_tool.ker_e must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_r < 0.0 OR NEW.ker_r > 1.0 THEN
            RAISE(ABORT, 'mcp_tool.ker_r must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_s != NEW.ker_k * NEW.ker_e - NEW.ker_r THEN
            RAISE(ABORT, 'mcp_tool.ker_s must equal ker_k * ker_e - ker_r')
    END;
END;

DROP TRIGGER IF EXISTS trg_mcp_tool_ker_invariants_update;

CREATE TRIGGER trg_mcp_tool_ker_invariants_update
BEFORE UPDATE ON mcp_tool
BEGIN
    SELECT CASE
        WHEN NEW.ker_k < 0.0 OR NEW.ker_k > 1.0 THEN
            RAISE(ABORT, 'mcp_tool.ker_k must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_e < 0.0 OR NEW.ker_e > 1.0 THEN
            RAISE(ABORT, 'mcp_tool.ker_e must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_r < 0.0 OR NEW.ker_r > 1.0 THEN
            RAISE(ABORT, 'mcp_tool.ker_r must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_s != NEW.ker_k * NEW.ker_e - NEW.ker_r THEN
            RAISE(ABORT, 'mcp_tool.ker_s must equal ker_k * ker_e - ker_r')
    END;
END;

-- Synapse governance and lane enforcement.

-- Analytics-only tools must be non-actuating.
DROP TRIGGER IF EXISTS trg_mcp_tool_analytics_nonactuating;

CREATE TRIGGER trg_mcp_tool_analytics_nonactuating
BEFORE INSERT ON mcp_tool
BEGIN
    SELECT CASE
        WHEN NEW.synapse_class = 'ANALYTIC_BRIDGE' AND NEW.allows_actuation = 1 THEN
            RAISE(ABORT, 'ANALYTIC_BRIDGE tools must not allow actuation')
    END;
    SELECT CASE
        WHEN NEW.synapse_class = 'ANALYTIC_BRIDGE' AND NEW.nonactuatingonly = 0 THEN
            RAISE(ABORT, 'ANALYTIC_BRIDGE tools must be nonactuatingonly = 1')
    END;
END;

DROP TRIGGER IF EXISTS trg_mcp_tool_analytics_nonactuating_update;

CREATE TRIGGER trg_mcp_tool_analytics_nonactuating_update
BEFORE UPDATE ON mcp_tool
BEGIN
    SELECT CASE
        WHEN NEW.synapse_class = 'ANALYTIC_BRIDGE' AND NEW.allows_actuation = 1 THEN
            RAISE(ABORT, 'ANALYTIC_BRIDGE tools must not allow actuation')
    END;
    SELECT CASE
        WHEN NEW.synapse_class = 'ANALYTIC_BRIDGE' AND NEW.nonactuatingonly = 0 THEN
            RAISE(ABORT, 'ANALYTIC_BRIDGE tools must be nonactuatingonly = 1')
    END;
END;

-- Neurorights corridor: neuro tools cannot be actuation gates.
DROP TRIGGER IF EXISTS trg_mcp_tool_neuro_corridor;

CREATE TRIGGER trg_mcp_tool_neuro_corridor
BEFORE INSERT ON mcp_tool
BEGIN
    SELECT CASE
        WHEN NEW.neuroflag = 1 AND NEW.synapse_class = 'ACTUATION_GATE' THEN
            RAISE(ABORT, 'Neuro-adjacent tools cannot be ACTUATION_GATE')
    END;
    SELECT CASE
        WHEN NEW.neuroflag = 1 AND NEW.consent_level != 'EXPLICIT' THEN
            RAISE(ABORT, 'Neuro-adjacent tools require consent_level = EXPLICIT')
    END;
END;

DROP TRIGGER IF EXISTS trg_mcp_tool_neuro_corridor_update;

CREATE TRIGGER trg_mcp_tool_neuro_corridor_update
BEFORE UPDATE ON mcp_tool
BEGIN
    SELECT CASE
        WHEN NEW.neuroflag = 1 AND NEW.synapse_class = 'ACTUATION_GATE' THEN
            RAISE(ABORT, 'Neuro-adjacent tools cannot be ACTUATION_GATE')
    END;
    SELECT CASE
        WHEN NEW.neuroflag = 1 AND NEW.consent_level != 'EXPLICIT' THEN
            RAISE(ABORT, 'Neuro-adjacent tools require consent_level = EXPLICIT')
    END;
END;

-- Lane enforcement: PROD tools require stronger KER and governance.
DROP TRIGGER IF EXISTS trg_mcp_tool_prod_lane;

CREATE TRIGGER trg_mcp_tool_prod_lane
BEFORE INSERT ON mcp_tool
BEGIN
    SELECT CASE
        WHEN NEW.lanedefault = 'PROD' AND NEW.ker_s <= 0.2 THEN
            RAISE(ABORT, 'PROD tools must have ker_s > 0.2')
    END;
    SELECT CASE
        WHEN NEW.lanedefault = 'PROD' AND NEW.ker_e < 0.7 THEN
            RAISE(ABORT, 'PROD tools must have ker_e >= 0.7')
    END;
    SELECT CASE
        WHEN NEW.lanedefault = 'PROD' AND NEW.ker_r > 0.5 THEN
            RAISE(ABORT, 'PROD tools must have ker_r <= 0.5')
    END;
    SELECT CASE
        WHEN NEW.lanedefault = 'PROD' AND NEW.citizen_ready = 0 THEN
            RAISE(ABORT, 'PROD tools must be citizen_ready = 1')
    END;
END;

DROP TRIGGER IF EXISTS trg_mcp_tool_prod_lane_update;

CREATE TRIGGER trg_mcp_tool_prod_lane_update
BEFORE UPDATE ON mcp_tool
BEGIN
    SELECT CASE
        WHEN NEW.lanedefault = 'PROD' AND NEW.ker_s <= 0.2 THEN
            RAISE(ABORT, 'PROD tools must have ker_s > 0.2')
    END;
    SELECT CASE
        WHEN NEW.lanedefault = 'PROD' AND NEW.ker_e < 0.7 THEN
            RAISE(ABORT, 'PROD tools must have ker_e >= 0.7')
    END;
    SELECT CASE
        WHEN NEW.lanedefault = 'PROD' AND NEW.ker_r > 0.5 THEN
            RAISE(ABORT, 'PROD tools must have ker_r <= 0.5')
    END;
    SELECT CASE
        WHEN NEW.lanedefault = 'PROD' AND NEW.citizen_ready = 0 THEN
            RAISE(ABORT, 'PROD tools must be citizen_ready = 1')
    END;
END;
