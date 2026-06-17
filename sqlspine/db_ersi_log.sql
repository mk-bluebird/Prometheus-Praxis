-- filename: ecorestorationshard/sqlspine/db_ersi_log.sql
PRAGMA foreign_keys = ON;

-- ERSI research-band logging table:
-- - Non-actuating diagnostics for AI interactions.
-- - Records tokens, compute joules, and K/E/R factors.
-- - Aligns with existing EcoNet research spine conventions.
CREATE TABLE IF NOT EXISTS ersi_log (
    ersi_id        TEXT NOT NULL,
    steward_did    TEXT NOT NULL,
    repo           TEXT NOT NULL,
    lane           TEXT NOT NULL,
    tokens_used    INTEGER NOT NULL CHECK(tokens_used >= 0),
    compute_joules REAL NOT NULL CHECK(compute_joules >= 0.0),
    kfactor        REAL NOT NULL CHECK(kfactor >= 0.0 AND kfactor <= 1.0),
    efactor        REAL NOT NULL CHECK(efactor >= 0.0 AND efactor <= 1.0),
    rfactor        REAL NOT NULL CHECK(rfactor >= 0.0 AND rfactor <= 1.0),
    topic          TEXT NOT NULL,
    created_utc    TEXT NOT NULL,
    PRIMARY KEY (ersi_id, created_utc)
);

CREATE INDEX IF NOT EXISTS idx_ersi_log_steward_time
    ON ersi_log (steward_did, created_utc);

CREATE INDEX IF NOT EXISTS idx_ersi_log_repo_time
    ON ersi_log (repo, created_utc);

CREATE INDEX IF NOT EXISTS idx_ersi_log_topic_time
    ON ersi_log (topic, created_utc);

-- Optional view: daily aggregates per steward and repo.
CREATE VIEW IF NOT EXISTS v_ersi_daily AS
SELECT
    steward_did,
    repo,
    substr(created_utc, 1, 10) AS day_utc,
    COUNT(*)                    AS n_events,
    SUM(tokens_used)            AS tokens_total,
    SUM(compute_joules)         AS joules_total,
    AVG(kfactor)                AS kfactor_avg,
    AVG(efactor)                AS efactor_avg,
    AVG(rfactor)                AS rfactor_avg
FROM ersi_log
GROUP BY steward_did, repo, day_utc;
