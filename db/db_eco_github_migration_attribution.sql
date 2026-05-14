-- filename db_eco_github_migration_attribution.sql
-- destination eco_restoration_shard/db/db_eco_github_migration_attribution.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 26. Timestamp-aware attribution for eco_github_migration + eco_author_evidence
-------------------------------------------------------------------------------
-- Assumptions:
--   eco_github_migration:
--     migration_id INTEGER PRIMARY KEY
--     legacy_login TEXT NOT NULL
--     current_login TEXT NOT NULL
--     effective_utc TEXT NOT NULL  -- existing
--     revoked INTEGER NOT NULL DEFAULT 0
--
--   eco_author_evidence:
--     evidence_id   INTEGER PRIMARY KEY
--     shardid       INTEGER NOT NULL REFERENCES shardinstance(shardid)
--     legacy_login  TEXT NOT NULL
--     current_login TEXT     -- nullable, filled post-migration
--     authored_utc  TEXT NOT NULL  -- ISO8601 time the shard was authored

-- 26.1 View to compute dynamic attribution based on authored_utc
CREATE VIEW IF NOT EXISTS v_author_attribution AS
WITH ordered_migrations AS (
    SELECT
        m.migration_id,
        m.legacy_login,
        m.current_login,
        m.effective_utc,
        m.revoked
    FROM eco_github_migration m
    WHERE m.revoked = 0
),
attributed_evidence AS (
    SELECT
        ae.evidence_id,
        ae.shardid,
        ae.legacy_login,
        ae.current_login,
        ae.authored_utc,
        om.current_login AS attributed_login
    FROM eco_author_evidence ae
    LEFT JOIN ordered_migrations om
      ON om.legacy_login = ae.legacy_login
     AND ae.authored_utc >= om.effective_utc
)
SELECT
    evidence_id,
    shardid,
    legacy_login,
    COALESCE(current_login, attributed_login) AS current_login,
    authored_utc
FROM attributed_evidence;

-- 26.2 Commentary:
--   Agents should read v_author_attribution instead of eco_author_evidence directly.
--   Contributions before effective_utc remain attributed to legacy_login.
--   Contributions on or after effective_utc are attributed to current_login.
