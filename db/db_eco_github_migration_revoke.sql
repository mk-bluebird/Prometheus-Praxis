-- filename db_eco_github_migration_revoke.sql
-- destination eco_restoration_shard/db/db_eco_github_migration_revoke.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Extend eco_github_migration with revoked state
-------------------------------------------------------------------------------

ALTER TABLE eco_github_migration
ADD COLUMN revoked INTEGER NOT NULL DEFAULT 0 CHECK (revoked IN (0,1));

ALTER TABLE eco_github_migration
ADD COLUMN revoked_reason TEXT;

ALTER TABLE eco_github_migration
ADD COLUMN revoked_utc TEXT;

-------------------------------------------------------------------------------
-- 2. Governance table for migration reversals
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_github_migration_verdict (
    verdict_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    migration_id     INTEGER NOT NULL REFERENCES eco_github_migration(migration_id)
                        ON DELETE CASCADE,
    verdict_kind     TEXT NOT NULL CHECK (verdict_kind IN (
                           'CONFIRM',
                           'REVOKE'
                       )),
    verdict_reason   TEXT NOT NULL,
    issued_by_did    TEXT NOT NULL,
    issued_utc       TEXT NOT NULL,
    evidencehex      TEXT NOT NULL,
    active           INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1))
);

CREATE INDEX IF NOT EXISTS idx_eco_github_migration_verdict_migration
    ON eco_github_migration_verdict(migration_id, active);

-------------------------------------------------------------------------------
-- 3. Trigger: applying a REVOKE verdict marks migration as revoked
-------------------------------------------------------------------------------

CREATE TRIGGER IF NOT EXISTS trg_eco_github_migration_revoke
AFTER INSERT ON eco_github_migration_verdict
BEGIN
    UPDATE eco_github_migration
    SET revoked = CASE
                      WHEN NEW.verdict_kind = 'REVOKE' THEN 1
                      ELSE revoked
                  END,
        revoked_reason = CASE
                             WHEN NEW.verdict_kind = 'REVOKE' THEN NEW.verdict_reason
                             ELSE revoked_reason
                         END,
        revoked_utc = CASE
                          WHEN NEW.verdict_kind = 'REVOKE' THEN NEW.issued_utc
                          ELSE revoked_utc
                      END
    WHERE migration_id = NEW.migration_id;
END;
