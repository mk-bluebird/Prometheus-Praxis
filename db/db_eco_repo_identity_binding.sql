-- filename db_eco_repo_identity_binding.sql
-- destination eco_restoration_shard/db/db_eco_repo_identity_binding.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. eco_repo_identity_binding with single-current-owner invariant
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_repo_identity_binding (
    binding_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_id          INTEGER NOT NULL REFERENCES repo(repoid) ON DELETE CASCADE,
    github_login     TEXT NOT NULL,        -- GitHub username
    binding_kind     TEXT NOT NULL CHECK (binding_kind IN (
                           'OWNER',
                           'COLLABORATOR',
                           'BOT',
                           'OTHER'
                       )),
    is_current_owner INTEGER NOT NULL DEFAULT 0 CHECK (is_current_owner IN (0,1)),
    evidencehex      TEXT NOT NULL,
    signingdid       TEXT NOT NULL,
    created_utc      TEXT NOT NULL,
    updated_utc      TEXT NOT NULL,
    active           INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),

    UNIQUE (repo_id, github_login)
);

-------------------------------------------------------------------------------
-- 1.1 Partial-like uniqueness: exactly one current owner per repo
--
-- SQLite does not support filtered UNIQUE indexes directly, so we
-- enforce the invariant via a BEFORE INSERT/UPDATE trigger:
--
--   For any row with is_current_owner = 1 and binding_kind = 'OWNER',
--   there must be no other active row for the same repo_id with
--   is_current_owner = 1.
-------------------------------------------------------------------------------

CREATE TRIGGER IF NOT EXISTS trg_eco_repo_identity_binding_one_owner_ins
BEFORE INSERT ON eco_repo_identity_binding
BEGIN
    SELECT
        CASE
            WHEN NEW.binding_kind = 'OWNER'
                 AND NEW.is_current_owner = 1
                 AND EXISTS (
                     SELECT 1
                     FROM eco_repo_identity_binding
                     WHERE repo_id = NEW.repo_id
                       AND binding_kind = 'OWNER'
                       AND is_current_owner = 1
                       AND active = 1
                 )
            THEN
                RAISE(ABORT, 'Invariant violation: repo already has a current owner.')
        END;
END;

CREATE TRIGGER IF NOT EXISTS trg_eco_repo_identity_binding_one_owner_upd
BEFORE UPDATE OF is_current_owner, binding_kind, active ON eco_repo_identity_binding
BEGIN
    -- If this row is being promoted to current owner, ensure uniqueness.
    SELECT
        CASE
            WHEN NEW.binding_kind = 'OWNER'
                 AND NEW.is_current_owner = 1
                 AND NEW.active = 1
                 AND EXISTS (
                     SELECT 1
                     FROM eco_repo_identity_binding
                     WHERE repo_id = NEW.repo_id
                       AND binding_kind = 'OWNER'
                       AND is_current_owner = 1
                       AND active = 1
                       AND binding_id <> NEW.binding_id
                 )
            THEN
                RAISE(ABORT, 'Invariant violation: repo already has a current owner.')
        END;
END;

CREATE INDEX IF NOT EXISTS idx_eco_repo_identity_binding_repo_owner
    ON eco_repo_identity_binding(repo_id, binding_kind, is_current_owner, active);
