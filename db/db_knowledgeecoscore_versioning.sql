-- filename db_knowledgeecoscore_versioning.sql
-- destination Eco-Fort/db/db_knowledgeecoscore_versioning.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Add score_version and effective_utc to knowledgeecoscore
-------------------------------------------------------------------------------

ALTER TABLE knowledgeecoscore
ADD COLUMN score_version INTEGER NOT NULL DEFAULT 1;

ALTER TABLE knowledgeecoscore
ADD COLUMN effective_utc TEXT NOT NULL DEFAULT (datetime('now'));

CREATE INDEX IF NOT EXISTS idx_knowledgeecoscore_scope_version
    ON knowledgeecoscore(scopetype, scoperefid, score_version, effective_utc);

-------------------------------------------------------------------------------
-- 2. Optional history table for superseded scores
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS knowledgeecoscore_history (
    history_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    scoreid       INTEGER NOT NULL,
    scopetype     TEXT NOT NULL,
    scoperefid    INTEGER NOT NULL,
    kfactor       REAL NOT NULL,
    efactor       REAL NOT NULL,
    rfactor       REAL NOT NULL,
    score_version INTEGER NOT NULL,
    effective_utc TEXT NOT NULL,
    superseded_utc TEXT NOT NULL,
    evidencehex   TEXT NOT NULL,
    signingdid    TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_knowledgeecoscore_history_scope
    ON knowledgeecoscore_history(scopetype, scoperefid, score_version);
