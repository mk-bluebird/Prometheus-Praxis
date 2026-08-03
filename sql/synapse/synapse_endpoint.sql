-- File: sql/synapse/synapse_endpoint.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/synapse/synapse_endpoint.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS synapse_endpoint (
    synapse_id        INTEGER PRIMARY KEY AUTOINCREMENT,

    producer_lang     TEXT NOT NULL CHECK (producer_lang IN ('CPP', 'JAVA', 'LUA', 'KOTLIN', 'SQL')),
    producer_relpath  TEXT NOT NULL,
    consumer_lang     TEXT NOT NULL CHECK (consumer_lang IN ('CPP', 'JAVA', 'LUA', 'KOTLIN', 'SQL')),
    consumer_relpath  TEXT NOT NULL,

    synapse_class     TEXT NOT NULL CHECK (synapse_class IN ('ANALYTIC_BRIDGE', 'ACTUATION_GATE', 'PURE_GOVERNANCE')),
    transport_kind    TEXT NOT NULL CHECK (transport_kind IN ('CLI_CSV', 'CLI_JSONL', 'JNI', 'LUA_FFI', 'KOTLIN_PROCESS')),

    lane_default      TEXT NOT NULL CHECK (lane_default IN ('RESEARCH', 'EXPPROD', 'PROD')),
    primary_plane     TEXT NOT NULL,  -- e.g. "CARBON", "HYDRAULICS", "NEURO"

    non_actuating     INTEGER NOT NULL DEFAULT 1 CHECK (non_actuating IN (0, 1)),

    ker_k             REAL NOT NULL DEFAULT 0.0,
    ker_e             REAL NOT NULL DEFAULT 0.0,
    ker_r             REAL NOT NULL DEFAULT 0.0,
    ker_s             REAL NOT NULL DEFAULT 0.0,

    allows_readonly   INTEGER NOT NULL DEFAULT 1 CHECK (allows_readonly IN (0, 1)),
    allows_actuation  INTEGER NOT NULL DEFAULT 0 CHECK (allows_actuation IN (0, 1)),
    neuro_flag        INTEGER NOT NULL DEFAULT 0 CHECK (neuro_flag IN (0, 1)),

    created_utc       TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc       TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE INDEX IF NOT EXISTS idx_synapse_relpaths
    ON synapse_endpoint (producer_relpath, consumer_relpath);

CREATE INDEX IF NOT EXISTS idx_synapse_lane_plane
    ON synapse_endpoint (lane_default, primary_plane);

-- SynapseKerBounds + SynapseKerScalarConsistency.

DROP TRIGGER IF EXISTS trg_synapse_ker_bounds_insert;

CREATE TRIGGER trg_synapse_ker_bounds_insert
BEFORE INSERT ON synapse_endpoint
BEGIN
    SELECT CASE
        WHEN NEW.ker_k < 0.0 OR NEW.ker_k > 1.0 THEN
            RAISE(ABORT, 'synapse_endpoint.ker_k must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_e < 0.0 OR NEW.ker_e > 1.0 THEN
            RAISE(ABORT, 'synapse_endpoint.ker_e must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_r < 0.0 OR NEW.ker_r > 1.0 THEN
            RAISE(ABORT, 'synapse_endpoint.ker_r must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_s != NEW.ker_k * NEW.ker_e - NEW.ker_r THEN
            RAISE(ABORT, 'synapse_endpoint.ker_s must equal ker_k * ker_e - ker_r')
    END;
END;

DROP TRIGGER IF EXISTS trg_synapse_ker_bounds_update;

CREATE TRIGGER trg_synapse_ker_bounds_update
BEFORE UPDATE ON synapse_endpoint
BEGIN
    SELECT CASE
        WHEN NEW.ker_k < 0.0 OR NEW.ker_k > 1.0 THEN
            RAISE(ABORT, 'synapse_endpoint.ker_k must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_e < 0.0 OR NEW.ker_e > 1.0 THEN
            RAISE(ABORT, 'synapse_endpoint.ker_e must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_r < 0.0 OR NEW.ker_r > 1.0 THEN
            RAISE(ABORT, 'synapse_endpoint.ker_r must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_s != NEW.ker_k * NEW.ker_e - NEW.ker_r THEN
            RAISE(ABORT, 'synapse_endpoint.ker_s must equal ker_k * ker_e - ker_r')
    END;
END;

-- AnalyticsBridgeIsNonActuating: ANALYTIC_BRIDGE => non_actuating=true, allows_readonly=true, allows_actuation=false.

DROP TRIGGER IF EXISTS trg_synapse_analytics_nonactuating_insert;

CREATE TRIGGER trg_synapse_analytics_nonactuating_insert
BEFORE INSERT ON synapse_endpoint
BEGIN
    SELECT CASE
        WHEN NEW.synapse_class = 'ANALYTIC_BRIDGE' AND NEW.non_actuating = 0 THEN
            RAISE(ABORT, 'ANALYTIC_BRIDGE synapses must be non_actuating = 1')
    END;
    SELECT CASE
        WHEN NEW.synapse_class = 'ANALYTIC_BRIDGE' AND NEW.allows_readonly = 0 THEN
            RAISE(ABORT, 'ANALYTIC_BRIDGE synapses must allow_readonly = 1')
    END;
    SELECT CASE
        WHEN NEW.synapse_class = 'ANALYTIC_BRIDGE' AND NEW.allows_actuation = 1 THEN
            RAISE(ABORT, 'ANALYTIC_BRIDGE synapses must not allow actuation')
    END;
END;

DROP TRIGGER IF EXISTS trg_synapse_analytics_nonactuating_update;

CREATE TRIGGER trg_synapse_analytics_nonactuating_update
BEFORE UPDATE ON synapse_endpoint
BEGIN
    SELECT CASE
        WHEN NEW.synapse_class = 'ANALYTIC_BRIDGE' AND NEW.non_actuating = 0 THEN
            RAISE(ABORT, 'ANALYTIC_BRIDGE synapses must be non_actuating = 1')
    END;
    SELECT CASE
        WHEN NEW.synapse_class = 'ANALYTIC_BRIDGE' AND NEW.allows_readonly = 0 THEN
            RAISE(ABORT, 'ANALYTIC_BRIDGE synapses must allow_readonly = 1')
    END;
    SELECT CASE
        WHEN NEW.synapse_class = 'ANALYTIC_BRIDGE' AND NEW.allows_actuation = 1 THEN
            RAISE(ABORT, 'ANALYTIC_BRIDGE synapses must not allow actuation')
    END;
END;

-- CliTransportNonActuating: CLI_* transports must always be non_actuating and not allow actuation.

DROP TRIGGER IF EXISTS trg_synapse_cli_nonactuating_insert;

CREATE TRIGGER trg_synapse_cli_nonactuating_insert
BEFORE INSERT ON synapse_endpoint
BEGIN
    SELECT CASE
        WHEN (NEW.transport_kind = 'CLI_CSV' OR NEW.transport_kind = 'CLI_JSONL')
             AND NEW.non_actuating = 0 THEN
            RAISE(ABORT, 'CLI_* synapses must be non_actuating = 1')
    END;
    SELECT CASE
        WHEN (NEW.transport_kind = 'CLI_CSV' OR NEW.transport_kind = 'CLI_JSONL')
             AND NEW.allows_actuation = 1 THEN
            RAISE(ABORT, 'CLI_* synapses must not allow actuation')
    END;
END;

DROP TRIGGER IF EXISTS trg_synapse_cli_nonactuating_update;

CREATE TRIGGER trg_synapse_cli_nonactuating_update
BEFORE UPDATE ON synapse_endpoint
BEGIN
    SELECT CASE
        WHEN (NEW.transport_kind = 'CLI_CSV' OR NEW.transport_kind = 'CLI_JSONL')
             AND NEW.non_actuating = 0 THEN
            RAISE(ABORT, 'CLI_* synapses must be non_actuating = 1')
    END;
    SELECT CASE
        WHEN (NEW.transport_kind = 'CLI_CSV' OR NEW.transport_kind = 'CLI_JSONL')
             AND NEW.allows_actuation = 1 THEN
            RAISE(ABORT, 'CLI_* synapses must not allow actuation')
    END;
END;

-- JniAndFfiKerThresholds: JNI/LUA_FFI/KOTLIN_PROCESS require ker_s > 0.1, ker_e >= 0.6, ker_r <= 0.4.

DROP TRIGGER IF EXISTS trg_synapse_jni_ffi_ker_insert;

CREATE TRIGGER trg_synapse_jni_ffi_ker_insert
BEFORE INSERT ON synapse_endpoint
BEGIN
    SELECT CASE
        WHEN (NEW.transport_kind IN ('JNI', 'LUA_FFI', 'KOTLIN_PROCESS'))
             AND (NEW.ker_s <= 0.1 OR NEW.ker_e < 0.6 OR NEW.ker_r > 0.4) THEN
            RAISE(ABORT, 'JNI/FFI synapses require ker_s > 0.1, ker_e >= 0.6, ker_r <= 0.4')
    END;
END;

DROP TRIGGER IF EXISTS trg_synapse_jni_ffi_ker_update;

CREATE TRIGGER trg_synapse_jni_ffi_ker_update
BEFORE UPDATE ON synapse_endpoint
BEGIN
    SELECT CASE
        WHEN (NEW.transport_kind IN ('JNI', 'LUA_FFI', 'KOTLIN_PROCESS'))
             AND (NEW.ker_s <= 0.1 OR NEW.ker_e < 0.6 OR NEW.ker_r > 0.4) THEN
            RAISE(ABORT, 'JNI/FFI synapses require ker_s > 0.1, ker_e >= 0.6, ker_r <= 0.4')
    END;
END;

-- SynapseProdLaneConstraints: PROD lane synapses must be non-actuating, not ACTUATION_GATE, and have strong KER.

DROP TRIGGER IF EXISTS trg_synapse_prod_lane_insert;

CREATE TRIGGER trg_synapse_prod_lane_insert
BEFORE INSERT ON synapse_endpoint
BEGIN
    SELECT CASE
        WHEN NEW.lane_default = 'PROD'
             AND (NEW.synapse_class = 'ACTUATION_GATE' OR NEW.non_actuating = 0) THEN
            RAISE(ABORT, 'PROD synapses must be non-actuating and not ACTUATION_GATE')
    END;
    SELECT CASE
        WHEN NEW.lane_default = 'PROD'
             AND (NEW.ker_s <= 0.2 OR NEW.ker_e < 0.7 OR NEW.ker_r > 0.5) THEN
            RAISE(ABORT, 'PROD synapses require ker_s > 0.2, ker_e >= 0.7, ker_r <= 0.5')
    END;
END;

DROP TRIGGER IF EXISTS trg_synapse_prod_lane_update;

CREATE TRIGGER trg_synapse_prod_lane_update
BEFORE UPDATE ON synapse_endpoint
BEGIN
    SELECT CASE
        WHEN NEW.lane_default = 'PROD'
             AND (NEW.synapse_class = 'ACTUATION_GATE' OR NEW.non_actuating = 0) THEN
            RAISE(ABORT, 'PROD synapses must be non-actuating and not ACTUATION_GATE')
    END;
    SELECT CASE
        WHEN NEW.lane_default = 'PROD'
             AND (NEW.ker_s <= 0.2 OR NEW.ker_e < 0.7 OR NEW.ker_r > 0.5) THEN
            RAISE(ABORT, 'PROD synapses require ker_s > 0.2, ker_e >= 0.7, ker_r <= 0.5')
    END;
END;

-- NeuroSynapseNonActuating: neuro_flag=1 must never be ACTUATION_GATE and must be non-actuating, not allow actuation.

DROP TRIGGER IF EXISTS trg_synapse_neuro_corridor_insert;

CREATE TRIGGER trg_synapse_neuro_corridor_insert
BEFORE INSERT ON synapse_endpoint
BEGIN
    SELECT CASE
        WHEN NEW.neuro_flag = 1 AND NEW.synapse_class = 'ACTUATION_GATE' THEN
            RAISE(ABORT, 'Neuro synapses cannot be ACTUATION_GATE')
    END;
    SELECT CASE
        WHEN NEW.neuro_flag = 1 AND (NEW.non_actuating = 0 OR NEW.allows_actuation = 1) THEN
            RAISE(ABORT, 'Neuro synapses must be non-actuating and not allow actuation')
    END;
END;

DROP TRIGGER IF EXISTS trg_synapse_neuro_corridor_update;

CREATE TRIGGER trg_synapse_neuro_corridor_update
BEFORE UPDATE ON synapse_endpoint
BEGIN
    SELECT CASE
        WHEN NEW.neuro_flag = 1 AND NEW.synapse_class = 'ACTUATION_GATE' THEN
            RAISE(ABORT, 'Neuro synapses cannot be ACTUATION_GATE')
    END;
    SELECT CASE
        WHEN NEW.neuro_flag = 1 AND (NEW.non_actuating = 0 OR NEW.allows_actuation = 1) THEN
            RAISE(ABORT, 'Neuro synapses must be non-actuating and not allow actuation')
    END;
END;

-- Auto-update updated_utc on write.

DROP TRIGGER IF EXISTS trg_synapse_endpoint_timestamp;

CREATE TRIGGER trg_synapse_endpoint_timestamp
AFTER UPDATE ON synapse_endpoint
BEGIN
    UPDATE synapse_endpoint
    SET updated_utc = (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
    WHERE synapse_id = NEW.synapse_id;
END;
