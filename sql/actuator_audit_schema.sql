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

COMMIT;
