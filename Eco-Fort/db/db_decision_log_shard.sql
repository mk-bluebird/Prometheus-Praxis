-- File: Eco-Fort/db/db_decision_log_shard.sql
-- DirClass: SQL

CREATE TABLE IF NOT EXISTS decision_log_shard (
    decisionid TEXT PRIMARY KEY,
    taskid TEXT NOT NULL,
    allowed INTEGER NOT NULL,
    reasons TEXT NOT NULL, -- JSON encoded
    hextrace TEXT NOT NULL,
    timestamputc TEXT NOT NULL,
    k_ker REAL NOT NULL,
    e_ker REAL NOT NULL,
    r_ker REAL NOT NULL,
    janus_veritas_ref TEXT NOT NULL,
    lyapunov_residual REAL NOT NULL,
    tsafe_margin REAL NOT NULL,
    active INTEGER DEFAULT 1
);

CREATE INDEX IF NOT EXISTS idx_decision_task ON decision_log_shard(taskid);
CREATE INDEX IF NOT EXISTS idx_decision_veritas ON decision_log_shard(janus_veritas_ref);
CREATE INDEX IF NOT EXISTS idx_decision_ker_band ON decision_log_shard(k_ker, e_ker, r_ker);
CREATE INDEX IF NOT EXISTS idx_decision_lyap ON decision_log_shard(lyapunov_residual);

-- Agent-facing Governance View
DROP VIEW IF EXISTS v_decision_governance;
CREATE VIEW v_decision_governance AS
SELECT 
    decisionid,
    taskid,
    CASE WHEN allowed = 1 THEN 'GRANTED' ELSE 'DENIED' END AS verdict,
    json_extract(reasons, '$.primary_cause') AS primary_cause,
    json_extract(reasons, '$.roh_violation') AS roh_violation,
    k_ker, e_ker, r_ker,
    lyapunov_residual,
    tsafe_margin,
    janus_veritas_ref
FROM decision_log_shard
WHERE active = 1;
