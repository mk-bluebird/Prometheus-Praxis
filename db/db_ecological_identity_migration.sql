-- filename: db_ecological_identity_migration.sql
-- destination: Eco-Fort/db/db_ecological_identity_migration.sql

PRAGMA foreign_keys = ON;

------------------------------------------------------------------
-- 1. Canonical identity table (persons, DIDs, addresses)
------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_identity_person (
    person_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    -- Human-readable, stable label for you as an augmented citizen.
    canonical_name     TEXT NOT NULL,        -- e.g. 'bostrom'
    display_name       TEXT,                -- e.g. 'mk-bluebird (bostrom)'
    created_utc        TEXT NOT NULL DEFAULT (datetime('now')),
    updated_utc        TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(canonical_name)
);

-- Each DID / address that can be used as an author identity.
CREATE TABLE IF NOT EXISTS eco_identity_address (
    address_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    person_id          INTEGER NOT NULL REFERENCES eco_identity_person(person_id)
                        ON DELETE CASCADE,
    addr_kind          TEXT NOT NULL,       -- 'BOSTROM', 'ALT_BOSTROM', 'SAFE_ALT', 'ERC20'
    addr_value         TEXT NOT NULL,       -- the actual address string
    is_primary         INTEGER NOT NULL DEFAULT 0 CHECK (is_primary IN (0,1)),
    is_active          INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0,1)),
    created_utc        TEXT NOT NULL DEFAULT (datetime('now')),
    updated_utc        TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(addr_kind, addr_value)
);

------------------------------------------------------------------
-- 2. GitHub account mapping (old → new)
------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_github_account (
    github_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    person_id          INTEGER NOT NULL REFERENCES eco_identity_person(person_id)
                        ON DELETE CASCADE,
    login              TEXT NOT NULL,       -- 'Doctor0Evil', 'mk-bluebird'
    is_active          INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0,1)),
    created_utc        TEXT NOT NULL DEFAULT (datetime('now')),
    updated_utc        TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(login)
);

-- Explicit mapping of “this GitHub move was a migration, not a new person”.
CREATE TABLE IF NOT EXISTS eco_github_migration (
    migration_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    person_id          INTEGER NOT NULL REFERENCES eco_identity_person(person_id)
                        ON DELETE CASCADE,
    old_login          TEXT NOT NULL,       -- 'Doctor0Evil'
    new_login          TEXT NOT NULL,       -- 'mk-bluebird'
    effective_utc      TEXT NOT NULL,       -- when migration took effect
    note               TEXT,               -- free-text explanation / governance note
    created_utc        TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(old_login, new_login)
);

------------------------------------------------------------------
-- 3. Repo registry extension: bind repos to identity + current login
--
-- If you already have a `repo` table from the constellation spine,
-- this can be applied as an additive extension.
------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_repo_identity_binding (
    binding_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_id            INTEGER NOT NULL,    -- FK to existing `repo.repoid`
    person_id          INTEGER NOT NULL REFERENCES eco_identity_person(person_id)
                        ON DELETE CASCADE,
    -- Snapshot of GitHub login currently owning the repo.
    github_login       TEXT NOT NULL,
    -- True if this binding is the currently valid owner record.
    is_current_owner   INTEGER NOT NULL DEFAULT 1 CHECK (is_current_owner IN (0,1)),
    created_utc        TEXT NOT NULL DEFAULT (datetime('now')),
    updated_utc        TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(repo_id, github_login)
);

------------------------------------------------------------------
-- 4. Author/contributor evidence table:
--    ties historical particles, shards, and commits to your identity.
--
-- This does NOT move data; it records that legacy “Doctor0Evil”
-- evidence is owned by the same eco_identity_person as “mk-bluebird”.
------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_author_evidence (
    evidence_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    person_id          INTEGER NOT NULL REFERENCES eco_identity_person(person_id)
                        ON DELETE CASCADE,
    -- Optional foreign keys into existing constellation tables:
    repo_id            INTEGER,            -- FK to `repo.repoid` if available
    shard_id           INTEGER,            -- FK to `shardinstance.shardid` if available
    schema_id          INTEGER,            -- FK to `alnschema.schemaid` if available
    particle_id        INTEGER,            -- FK to `alnparticle.particleid` if available

    -- Raw source tags so you can attach evidence even when you
    -- do not yet have the numeric FK.
    source_kind        TEXT NOT NULL,      -- 'GIT_COMMIT', 'ALN_PARTICLE', 'QPUDATASHARD', 'DOC'
    source_locator     TEXT NOT NULL,      -- e.g. 'Doctor0Evil/EcoNet-CEIM-PhoenixWater@<commit>'
                                           -- or 'HydrologicalBufferPhoenix2026v1.aln'

    -- The GitHub login present at the time (may be legacy).
    legacy_github      TEXT NOT NULL,      -- usually 'Doctor0Evil'
    -- The canonical, current login for forward use.
    canonical_github   TEXT NOT NULL,      -- 'mk-bluebird'

    -- Bostrom / chain identity that signed the shard (if any).
    primary_addr_id    INTEGER REFERENCES eco_identity_address(address_id)
                        ON DELETE SET NULL,
    -- Snapshot of the address string at the time (for auditability).
    primary_addr_value TEXT,
    -- Optional evidence hash or DID string already in your shardinstance schema.
    signing_did        TEXT,

    created_utc        TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(source_kind, source_locator, legacy_github)
);

------------------------------------------------------------------
-- 5. Convenience view: “who is this author?” for agents
--
-- Agents can join this with `shardinstance`, `repo`, `alnschema`, etc.
------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_eco_author_identity AS
SELECT
    p.person_id,
    p.canonical_name,
    p.display_name,
    a.addr_kind,
    a.addr_value,
    a.is_primary,
    g.login            AS github_login,
    g.is_active        AS github_active
FROM eco_identity_person AS p
LEFT JOIN eco_identity_address AS a
    ON a.person_id = p.person_id
LEFT JOIN eco_github_account AS g
    ON g.person_id = p.person_id;

------------------------------------------------------------------
-- 6. Seed data for your specific case
--
-- NOTE: run these inserts once; subsequent migrations should use
--       UPDATEs / additional rows, not change these literals.
------------------------------------------------------------------

-- 6.1 Canonical person row for your Bostrom identity.
INSERT OR IGNORE INTO eco_identity_person (canonical_name, display_name)
VALUES ('bostrom', 'mk-bluebird (bostrom)');

-- Anchor variable for reuse.
WITH me AS (
    SELECT person_id FROM eco_identity_person WHERE canonical_name = 'bostrom'
)
INSERT OR IGNORE INTO eco_identity_address (
    person_id, addr_kind, addr_value, is_primary
)
SELECT person_id, 'BOSTROM',
       'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
       1
FROM me;

WITH me AS (
    SELECT person_id FROM eco_identity_person WHERE canonical_name = 'bostrom'
)
INSERT OR IGNORE INTO eco_identity_address (
    person_id, addr_kind, addr_value, is_primary
)
SELECT person_id, 'ALT_BOSTROM',
       'bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc',
       0
FROM me;

WITH me AS (
    SELECT person_id FROM eco_identity_person WHERE canonical_name = 'bostrom'
)
INSERT OR IGNORE INTO eco_identity_address (
    person_id, addr_kind, addr_value, is_primary
)
SELECT person_id, 'SAFE_ALT',
       'zeta12x0up66pzyeretzyku8p4ccuxrjqtqpdc4y4x8',
       0
FROM me;

WITH me AS (
    SELECT person_id FROM eco_identity_person WHERE canonical_name = 'bostrom'
)
INSERT OR IGNORE INTO eco_identity_address (
    person_id, addr_kind, addr_value, is_primary
)
SELECT person_id, 'ERC20',
       '0x519fC0eB4111323Cac44b70e1aE31c30e405802D',
       0
FROM me;

-- 6.2 GitHub accounts: legacy and new.
WITH me AS (
    SELECT person_id FROM eco_identity_person WHERE canonical_name = 'bostrom'
)
INSERT OR IGNORE INTO eco_github_account (person_id, login, is_active)
SELECT person_id, 'Doctor0Evil', 0 FROM me;

WITH me AS (
    SELECT person_id FROM eco_identity_person WHERE canonical_name = 'bostrom'
)
INSERT OR IGNORE INTO eco_github_account (person_id, login, is_active)
SELECT person_id, 'mk-bluebird', 1 FROM me;

-- 6.3 Migration record (logical “carry-over” of all records).
WITH me AS (
    SELECT person_id FROM eco_identity_person WHERE canonical_name = 'bostrom'
)
INSERT OR IGNORE INTO eco_github_migration (
    person_id, old_login, new_login, effective_utc, note
)
SELECT person_id,
       'Doctor0Evil',
       'mk-bluebird',
       '2026-05-01T00:00:00Z',
       'Migration of EcoNet / EcoFort / ecological-orchestrator constellation from Doctor0Evil to mk-bluebird with Bostrom-linked identity.'
FROM me;
