-- filename: ecosafety_core_v2/sql/governance_journal_and_raft.sql
-- destination: ecosafety_core_v2/sql/governance_journal_and_raft.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
--
-- Purpose:
--   SQLite schema for:
--     - Governance journal (event-sourced ALN particle state machines).
--     - Raft log entries for lane promotions across canal nodes.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Governance journal for ALN particle actors
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS governance_journal (
    journal_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    particle_id     TEXT    NOT NULL,
    event_index     INTEGER NOT NULL,    -- monotonically increasing per particle
    event_type      TEXT    NOT NULL,    -- e.g. 'WindowEvaluated', 'LanePromoted'
    event_payload   TEXT    NOT NULL,    -- JSON or ALN snippet
    evidence_hex    TEXT    NOT NULL,
    created_utc     TEXT    NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_gov_journal_particle_event
    ON governance_journal (particle_id, event_index);

----------------------------------------------------------------------
-- 2. Raft log entries for governance lane promotions
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS governance_raft_log (
    log_index       INTEGER PRIMARY KEY AUTOINCREMENT,
    term            INTEGER NOT NULL,
    cluster_id      TEXT    NOT NULL,  -- e.g. 'PHX-CANAL-CLUSTER-001'
    node_id         TEXT    NOT NULL,  -- canal node id
    particle_id     TEXT    NOT NULL,
    lane_prev       TEXT    NOT NULL,
    lane_next       TEXT    NOT NULL,
    evidence_hex    TEXT    NOT NULL,  -- dailyprogress / residual window hex
    vt_residual     REAL    NOT NULL,
    K               REAL    NOT NULL,
    E               REAL    NOT NULL,
    R               REAL    NOT NULL,
    signing_did     TEXT    NOT NULL,
    signature_hex   TEXT    NOT NULL,  -- digital signature over the entry
    created_utc     TEXT    NOT NULL,
    CHECK (K >= 0.0 AND K <= 1.0),
    CHECK (E >= 0.0 AND E <= 1.0),
    CHECK (R >= 0.0 AND R <= 1.0)
);

CREATE INDEX IF NOT EXISTS idx_raft_cluster_term
    ON governance_raft_log (cluster_id, term, log_index);
