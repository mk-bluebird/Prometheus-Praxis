-- filename: governance/db_contribution_ledger.sql
-- destination: eco_restoration_shard/governance/db_contribution_ledger.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS contribution_ledger (
    eventid INTEGER PRIMARY KEY AUTOINCREMENT,
    policyid TEXT NOT NULL,
    party_did TEXT NOT NULL,
    party_bostrom_address TEXT,
    hostdid TEXT NOT NULL,
    event_kind TEXT NOT NULL,
    file_path TEXT NOT NULL,
    file_extension TEXT NOT NULL,
    workflow_id TEXT NOT NULL,
    contribution_type TEXT NOT NULL,
    contribution_detail TEXT,
    contribution_target TEXT NOT NULL,
    violation_detail TEXT,
    director_notified INTEGER NOT NULL CHECK (director_notified IN (0, 1)),
    timestamp_utc TEXT NOT NULL,
    evidencehex TEXT NOT NULL,
    signinghex TEXT NOT NULL,
    FOREIGN KEY (policyid) REFERENCES absolute_data_sovereignty_policy (policyid)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_contribution_ledger_policy
    ON contribution_ledger (policyid);

CREATE INDEX IF NOT EXISTS idx_contribution_ledger_workflow
    ON contribution_ledger (workflow_id, timestamp_utc);

CREATE INDEX IF NOT EXISTS idx_contribution_ledger_event_kind
    ON contribution_ledger (event_kind);

CREATE VIEW IF NOT EXISTS v_contribution_summary AS
SELECT
    policyid,
    workflow_id,
    event_kind,
    COUNT(*) AS event_count
FROM contribution_ledger
GROUP BY policyid, workflow_id, event_kind;
