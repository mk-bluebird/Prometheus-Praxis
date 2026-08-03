-- File: sql/actuator_audit_schema.sql
-- Destination: Prometheus-Praxis/sql/actuator_audit_schema.sql

PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS actuator_audit (
    audit_id            INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp_utc       TEXT NOT NULL,
    deployment_id       TEXT NOT NULL,
    actuator_id         TEXT NOT NULL,
    command             TEXT NOT NULL,
    outcome             TEXT NOT NULL, -- e.g., ALLOW, DENY_NON_ACTUATING_ONLY
    non_actuating_only  INTEGER NOT NULL CHECK (non_actuating_only IN (0,1)),
    evidence_hex        TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_actuator_audit_deployment
    ON actuator_audit(deployment_id, timestamp_utc);

-- Actuation request table with Lyapunov-KER corridor enforcement
CREATE TABLE IF NOT EXISTS actuation_request (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    hex_id      TEXT NOT NULL,
    timestamp_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    dvt_pred    REAL NOT NULL,
    vt_before   REAL NOT NULL,
    vt_after    REAL NOT NULL,
    corridor_ok INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_actuation_request_hex
    ON actuation_request(hex_id, timestamp_utc);

-- Trigger to enforce Lyapunov-KER corridor before insert
CREATE TRIGGER IF NOT EXISTS actuation_request_corridor_enforce
BEFORE INSERT ON actuation_request
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.corridor_ok = 0 THEN
            RAISE(ABORT, 'Lyapunov-KER corridor violation: actuation blocked')
    END;
END;

COMMIT;
