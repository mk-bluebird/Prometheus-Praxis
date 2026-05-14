-- filename db_eco_github_migration.sql
-- destination eco_restoration_shard/db/db_eco_github_migration.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Author identity spine (DID-centric, GitHub as evidence only)
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_author_identity (
    author_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    signingdid    TEXT NOT NULL,
    primary_email TEXT,
    display_name  TEXT,
    created_utc   TEXT NOT NULL,
    updated_utc   TEXT NOT NULL,
    active        INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    UNIQUE(signingdid)
);

CREATE INDEX IF NOT EXISTS idx_eco_author_identity_signingdid
    ON eco_author_identity (signingdid, active);

INSERT OR IGNORE INTO eco_author_identity (
    signingdid,
    primary_email,
    display_name,
    created_utc,
    updated_utc,
    active
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
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
    gh_account_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    author_id       INTEGER NOT NULL
                        REFERENCES eco_author_identity(author_id)
                        ON DELETE CASCADE,
    github_username TEXT NOT NULL,
    github_slug     TEXT,
    account_state   TEXT NOT NULL CHECK (account_state IN (
                        'ACTIVE',
                        'LOST',
                        'DEPRECATED',
                        'FUTURE',
                        'UNKNOWN'
                    )),
    first_seen_utc  TEXT NOT NULL,
    last_seen_utc   TEXT NOT NULL,
    evidencehex     TEXT NOT NULL,
    rohanchorhex    TEXT,
    active          INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    UNIQUE(author_id, github_username)
);

CREATE INDEX IF NOT EXISTS idx_eco_github_account_author
    ON eco_github_account (author_id, account_state, active);

CREATE INDEX IF NOT EXISTS idx_eco_github_account_username
    ON eco_github_account (github_username);

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
    migration_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    author_id           INTEGER NOT NULL
                            REFERENCES eco_author_identity(author_id)
                            ON DELETE CASCADE,
    from_gh_account_id  INTEGER NOT NULL
                            REFERENCES eco_github_account(gh_account_id)
                            ON DELETE CASCADE,
    to_gh_account_id    INTEGER NOT NULL
                            REFERENCES eco_github_account(gh_account_id)
                            ON DELETE CASCADE,
    step_index          INTEGER NOT NULL,
    migration_reason    TEXT NOT NULL,
    effective_from_utc  TEXT NOT NULL,
    effective_until_utc TEXT,
    evidencehex         TEXT NOT NULL,
    rohanchorhex        TEXT,
    signingdid          TEXT NOT NULL,
    created_utc         TEXT NOT NULL,
    active              INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    UNIQUE (author_id, from_gh_account_id, to_gh_account_id, step_index)
);

CREATE INDEX IF NOT EXISTS idx_eco_github_migration_author_step
    ON eco_github_migration (author_id, step_index, active);

CREATE INDEX IF NOT EXISTS idx_eco_github_migration_from
    ON eco_github_migration (from_gh_account_id, active);

CREATE INDEX IF NOT EXISTS idx_eco_github_migration_to
    ON eco_github_migration (to_gh_account_id, active);

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
-- 4. Author evidence registry (GitHub, AI sessions, manual attestations)
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_author_evidence (
    evidence_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    author_id      INTEGER NOT NULL
                       REFERENCES eco_author_identity(author_id)
                       ON DELETE CASCADE,
    source_kind    TEXT NOT NULL CHECK (source_kind IN (
                       'GITHUB_COMMIT',
                       'GITHUB_REPO_LISTING',
                       'AI_SESSION_LOG',
                       'MANUAL_ATTESTATION',
                       'OTHER'
                   )),
    source_locator TEXT NOT NULL,
    legacy_github  TEXT,
    repo_name      TEXT,
    file_path      TEXT,
    shardkind      TEXT,
    lane           TEXT,
    evidencehex    TEXT NOT NULL,
    rohanchorhex   TEXT,
    created_utc    TEXT NOT NULL,
    active         INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    UNIQUE (source_kind, source_locator, legacy_github)
);

CREATE INDEX IF NOT EXISTS idx_eco_author_evidence_author
    ON eco_author_evidence (author_id, active);

CREATE INDEX IF NOT EXISTS idx_eco_author_evidence_repo
    ON eco_author_evidence (repo_name, shardkind, lane);

CREATE INDEX IF NOT EXISTS idx_eco_author_evidence_legacy
    ON eco_author_evidence (legacy_github);

-------------------------------------------------------------------------------
-- 5. View: multi-step migration path for an author
-------------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_eco_github_migration_chain AS
SELECT
    ai.signingdid           AS signingdid,
    ai.author_id            AS author_id,
    mg.migration_id         AS migration_id,
    mg.step_index           AS step_index,
    gh_from.github_username AS from_username,
    gh_to.github_username   AS to_username,
    mg.migration_reason     AS migration_reason,
    mg.effective_from_utc   AS effective_from_utc,
    mg.effective_until_utc  AS effective_until_utc,
    mg.evidencehex          AS evidencehex,
    mg.rohanchorhex         AS rohanchorhex,
    mg.signingdid           AS migration_signingdid
FROM eco_github_migration AS mg
JOIN eco_author_identity AS ai
  ON ai.author_id = mg.author_id
JOIN eco_github_account AS gh_from
  ON gh_from.gh_account_id = mg.from_gh_account_id
JOIN eco_github_account AS gh_to
  ON gh_to.gh_account_id = mg.to_gh_account_id
WHERE mg.active = 1
  AND ai.active = 1;
