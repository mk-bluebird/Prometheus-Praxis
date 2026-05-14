-- filename db_eco_github_account_merge.sql
-- destination eco_restoration_shard/db/db_eco_github_account_merge.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 46. eco_github_migration with multi-source merges
-------------------------------------------------------------------------------

-- Base table (existing):
--   eco_github_migration(
--     migration_id   INTEGER PRIMARY KEY AUTOINCREMENT,
--     legacy_login   TEXT NOT NULL,
--     current_login  TEXT NOT NULL,
--     effective_utc  TEXT NOT NULL,
--     revoked        INTEGER NOT NULL DEFAULT 0 CHECK (revoked IN (0,1))
--   );

-- New table to model merges of multiple legacy accounts into a single current account.
CREATE TABLE IF NOT EXISTS eco_github_account_merge (
    merge_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    legacy_login    TEXT NOT NULL,
    current_login   TEXT NOT NULL,
    effective_utc   TEXT NOT NULL,
    rationale       TEXT,
    created_utc     TEXT NOT NULL DEFAULT (datetime('now')),
    CONSTRAINT uq_merge_unique UNIQUE (legacy_login, current_login, effective_utc)
);

-- Guidance:
--   - eco_github_migration keeps the canonical one-to-one mapping used by
--     automated attribution (legacy_login -> current_login).
--   - eco_github_account_merge records that multiple legacy accounts
--     (e.g., 'Doctor0Evil', 'OtherAlias') converge into the same
--     current_login ('mk-bluebird') without overwriting the original
--     eco_github_migration rows.
--   - Provenance is preserved because each eco_author_evidence row still
--     carries legacy_login; merges are additional metadata, not rewrites.
