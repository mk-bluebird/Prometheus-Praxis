-- File: sql/governance_query_audit.sql
-- Audit table for MCP governance query logging with neurorights metadata

PRAGMA foreign_keys = ON;

-- Main audit table for all MCP/AI-chat queries
CREATE TABLE IF NOT EXISTS governance_query_audit (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp_utc   TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    tool_name       TEXT NOT NULL,
    caller_id       TEXT,
    ker_k           REAL,
    ker_e           REAL,
    ker_r           REAL,
    ker_s           REAL,
    neuro_flag      INTEGER DEFAULT 0,
    lane_default    TEXT,
    query_payload   TEXT
);

CREATE INDEX IF NOT EXISTS idx_gqa_timestamp
    ON governance_query_audit (timestamp_utc);

CREATE INDEX IF NOT EXISTS idx_gqa_tool_name
    ON governance_query_audit (tool_name);

CREATE INDEX IF NOT EXISTS idx_gqa_neuro_flag
    ON governance_query_audit (neuro_flag);

CREATE INDEX IF NOT EXISTS idx_gqa_lane_default
    ON governance_query_audit (lane_default);

-- Daily neurorights stats view
CREATE VIEW IF NOT EXISTS v_neurorights_query_stats AS
SELECT
    date(timestamp_utc) AS day,
    COUNT(*) AS total_queries,
    SUM(CASE WHEN neuro_flag = 1 THEN 1 ELSE 0 END) AS neuro_queries,
    SUM(CASE WHEN lane_default = 'PROD' AND neuro_flag = 1 THEN 1 ELSE 0 END) AS neuro_prod_queries
FROM governance_query_audit
GROUP BY date(timestamp_utc);
