-- filename db_eco_github_migration.sql
-- destination eco_restoration_shard/db/db_eco_github_migration.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Author identity spine (DID-centric, GitHub as evidence only)
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_author_identity (
    author_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    signingdid       TEXT NOT NULL,         -- Bostrom DID or equivalent
    primary_email    TEXT,                  -- optional non-canonical alias
    display_name     TEXT,                  -- human-readable name/handle
    created_utc      TEXT NOT NULL,         -- ISO8601
    updated_utc      TEXT NOT NULL,         -- ISO8601
    active           INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    UNIQUE(signingdid)
);

CREATE INDEX IF NOT EXISTS idx_author_identity_signingdid
    ON eco_author_identity(signingdid, active);


-------------------------------------------------------------------------------
-- Seed row for your Bostrom DID (optional helper insert)
-- NOTE: run once during initialization; future updates should go through code.
-------------------------------------------------------------------------------

INSERT OR IGNORE INTO eco_author_identity (
    signingdid,
    primary_email,
    display_name,
    created_utc,
    updated_utc,
    active
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',  -- your Bostrom delegator address
    NULL,
    'mk-bluebird',
    strftime('%Y-%m-%dT%H:%M:%SZ', 'now'),
    strftime('%Y-%m-%dT%H:%M:%SZ', 'now'),
    1
);


-------------------------------------------------------------------------------
-- 2. GitHub account registry (multi-step, DID-anchored)
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_github_account (
    gh_account_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    author_id        INTEGER NOT NULL REFERENCES eco_author_identity(author_id)
                        ON DELETE CASCADE,
    github_username  TEXT NOT NULL,         -- e.g. Doctor0Evil, mk-bluebird
    github_slug      TEXT,                  -- e.g. Doctor0Evil/EcoNet
    account_state    TEXT NOT NULL CHECK (account_state IN (
                          'ACTIVE',
                          'LOST',
                          'DEPRECATED',
                          'FUTURE',
                          'UNKNOWN'
                      )),
    first_seen_utc   TEXT NOT NULL,
    last_seen_utc    TEXT NOT NULL,
    evidencehex      TEXT NOT NULL,         -- hex descriptor of evidence bundle
    rohanchorhex     TEXT,                  -- optional RoH anchor over evidence
    active           INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    UNIQUE(author_id, github_username)
);

CREATE INDEX IF NOT EXISTS idx_eco_github_account_author
    ON eco_github_account(author_id, account_state, active);

CREATE INDEX IF NOT EXISTS idx_eco_github_account_username
    ON eco_github_account(github_username);


-------------------------------------------------------------------------------
-- 2.1 Optional seed rows for Doctor0Evil and mk-bluebird
--     (replace timestamps/evidencehex with real values when wiring CI)
-------------------------------------------------------------------------------

WITH me AS (
    SELECT author_id
    FROM eco_author_identity
    WHERE signingdid = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
      AND active = 1
    LIMIT 1
)
INSERT OR IGNORE INTO eco_github_account (
    author_id,
    github_username,
    github_slug,
    account_state,
    first_seen_utc,
    last_seen_utc,
    evidencehex,
    rohanchorhex,
    active
)
SELECT
    me.author_id,
    'Doctor0Evil',
    NULL,
    'LOST',
    '2020-01-01T00:00:00Z',
    '2024-01-01T00:00:00Z',
    'HEX_DOCTOR0EVIL_ACCOUNT_EVIDENCE',
    NULL,
    1
FROM me
UNION ALL
SELECT
    me.author_id,
    'mk-bluebird',
    'mk-bluebird/eco_restoration_shard',
    'ACTIVE',
    '2024-12-01T00:00:00Z',
    strftime('%Y-%m-%dT%H:%M:%SZ', 'now'),
    'HEX_MK_BLUEBIRD_ACCOUNT_EVIDENCE',
    NULL,
    1
FROM me;


-------------------------------------------------------------------------------
-- 3. GitHub migration chain (multi-step, append-only)
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_github_migration (
    migration_id           INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Logical author anchor
    author_id              INTEGER NOT NULL REFERENCES eco_author_identity(author_id)
                               ON DELETE CASCADE,

    -- From -> To accounts (can be chained)
    from_gh_account_id     INTEGER NOT NULL REFERENCES eco_github_account(gh_account_id)
                               ON DELETE CASCADE,
    to_gh_account_id       INTEGER NOT NULL REFERENCES eco_github_account(gh_account_id)
                               ON DELETE CASCADE,

    step_index             INTEGER NOT NULL,   -- 0,1,2,... monotone per author

    -- Migration semantics
    migration_reason       TEXT NOT NULL,      -- LOST_ACCOUNT, MERGE, FUTURE_MOVE
    effective_from_utc     TEXT NOT NULL,
    effective_until_utc    TEXT,              -- NULL means still in force

    -- Evidence and anchors
    evidencehex            TEXT NOT NULL,     -- links to eco_author_evidence rows
    rohanchorhex           TEXT,              -- optional anchor over all evidence
    signingdid             TEXT NOT NULL,     -- DID that authorized this migration

    created_utc            TEXT NOT NULL,
    active                 INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),

    -- Invariant: a given author/from/to pair cannot be duplicated.
    UNIQUE(author_id, from_gh_account_id, to_gh_account_id, step_index)
);

CREATE INDEX IF NOT EXISTS idx_eco_github_migration_author_step
    ON eco_github_migration(author_id, step_index, active);

CREATE INDEX IF NOT EXISTS idx_eco_github_migration_from
    ON eco_github_migration(from_gh_account_id, active);

CREATE INDEX IF NOT EXISTS idx_eco_github_migration_to
    ON eco_github_migration(to_gh_account_id, active);


-------------------------------------------------------------------------------
-- 3.1 Optional seed migration: Doctor0Evil -> mk-bluebird
-------------------------------------------------------------------------------

WITH
    me AS (
        SELECT author_id
        FROM eco_author_identity
        WHERE signingdid = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
          AND active = 1
        LIMIT 1
    ),
    gh_from AS (
        SELECT gh_account_id
        FROM eco_github_account
        WHERE github_username = 'Doctor0Evil'
          AND author_id = (SELECT author_id FROM me)
        LIMIT 1
    ),
    gh_to AS (
        SELECT gh_account_id
        FROM eco_github_account
        WHERE github_username = 'mk-bluebird'
          AND author_id = (SELECT author_id FROM me)
        LIMIT 1
    )
INSERT OR IGNORE INTO eco_github_migration (
    author_id,
    from_gh_account_id,
    to_gh_account_id,
    step_index,
    migration_reason,
    effective_from_utc,
    effective_until_utc,
    evidencehex,
    rohanchorhex,
    signingdid,
    created_utc,
    active
)
SELECT
    me.author_id,
    gh_from.gh_account_id,
    gh_to.gh_account_id,
    0,
    'LOST_ACCOUNT',
    '2024-12-15T00:00:00Z',
    NULL,
    'HEX_MIGRATION_DOCTOR0EVIL_TO_MK_BLUEBIRD',
    NULL,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    strftime('%Y-%m-%dT%H:%M:%SZ', 'now'),
    1
FROM me, gh_from, gh_to;


-------------------------------------------------------------------------------
-- 4. Author evidence registry (GitHub, Perplexity, etc.)
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_author_evidence (
    evidence_id      INTEGER PRIMARY KEY AUTOINCREMENT,

    author_id        INTEGER NOT NULL REFERENCES eco_author_identity(author_id)
                        ON DELETE CASCADE,

    -- Evidence source labeling
    source_kind      TEXT NOT NULL CHECK (source_kind IN (
                          'GITHUB_COMMIT',
                          'GITHUB_REPO_LISTING',
                          'AI_SESSION_LOG',
                          'MANUAL_ATTESTATION',
                          'OTHER'
                      )),

    -- Locator is a stable pointer for this source
    -- For GitHub: https://github.com/<user>/<repo>/commit/<sha>
    -- For AI logs: URI or internal log-id
    source_locator   TEXT NOT NULL,

    -- Legacy GitHub username this evidence was tied to when observed
    legacy_github    TEXT,                  -- e.g. Doctor0Evil

    -- Optional fields to help indexing
    repo_name        TEXT,                  -- logical repo name, e.g. EcoNet
    file_path        TEXT,                  -- relative file path if applicable
    shardkind        TEXT,                  -- QPUDATASHARD, SCHEMA, CODE, DOC
    lane             TEXT,                  -- RESEARCH, EXPPROD, PROD, etc.

    -- Evidence bundle hash and RoH anchors
    evidencehex      TEXT NOT NULL,         -- hash over the actual artifact
    rohanchorhex     TEXT,                  -- anchor into artifactregistry/ledger
    created_utc      TEXT NOT NULL,
    active           INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),

    -- Uniqueness: same external artifact cannot be double-registered.
    UNIQUE(source_kind, source_locator, legacy_github)
);

CREATE INDEX IF NOT EXISTS idx_eco_author_evidence_author
    ON eco_author_evidence(author_id, active);

CREATE INDEX IF NOT EXISTS idx_eco_author_evidence_repo
    ON eco_author_evidence(repo_name, shardkind, lane);

CREATE INDEX IF NOT EXISTS idx_eco_author_evidence_legacy
    ON eco_author_evidence(legacy_github);


-------------------------------------------------------------------------------
-- 5. View: multi-step migration path for an author
-------------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_eco_github_migration_chain AS
SELECT
    ai.signingdid              AS signingdid,
    ai.author_id               AS author_id,
    mg.migration_id            AS migration_id,
    mg.step_index              AS step_index,
    gh_from.github_username    AS from_username,
    gh_to.github_username      AS to_username,
    mg.migration_reason        AS migration_reason,
    mg.effective_from_utc      AS effective_from_utc,
    mg.effective_until_utc     AS effective_until_utc,
    mg.evidencehex             AS evidencehex,
    mg.rohanchorhex            AS rohanchorhex,
    mg.signingdid              AS migration_signingdid
FROM eco_github_migration mg
JOIN eco_author_identity ai
  ON ai.author_id = mg.author_id
JOIN eco_github_account gh_from
  ON gh_from.gh_account_id = mg.from_gh_account_id
JOIN eco_github_account gh_to
  ON gh_to.gh_account_id = mg.to_gh_account_id
WHERE mg.active = 1 AND ai.active = 1;
