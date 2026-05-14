-- filename: tasks_eco_restoration_shard_setup.sql
-- destination: eco_restoration_shard/db/tasks_eco_restoration_shard_setup.sql
--
-- NOTE TO CODER:
-- - Do NOT install Rustup or Cargo for this session.
-- - Use only SQLite CLI tools (sqlite3) and existing shell tooling.
-- - These tasks assume you clone eco_restoration_shard from:
--     https://github.com/mk-bluebird/eco_restoration_shard
-- - All actions are non-actuating and governance-only.

----------------------------------------------------------------------
-- 0. Session prerequisites (manual shell steps, not SQL)
----------------------------------------------------------------------
-- 0.1 Clone the repo (if not already present)
--   git clone https://github.com/mk-bluebird/eco_restoration_shard.git
--   cd eco_restoration_shard
--
-- 0.2 Ensure you have sqlite3 in PATH and verify version:
--   sqlite3 -version
--
-- 0.3 Choose or create a constellation DB path for this repo:
--   mkdir -p db
--   sqlite3 db/eco_constellation.db ".databases"

----------------------------------------------------------------------
-- 1. Create / attach the ecological identity spine
--
-- Goal: load the eco-identity migration tables into this repo’s DB,
-- so eco_restoration_shard can see the person → GitHub → Bostrom mapping
-- and reuse it across the constellation.
--
-- ACTION 1.a: save the eco-identity migration SQL (previous answer)
--             as db/db_ecological_identity_migration.sql in Eco-Fort
--             OR copy it into this repo’s db directory.
--
-- From the shell:
--   sqlite3 db/eco_constellation.db \
--     ".read db/db_ecological_identity_migration.sql"
--
-- This will create:
--   eco_identity_person
--   eco_identity_address
--   eco_github_account
--   eco_github_migration
--   eco_repo_identity_binding
--   eco_author_evidence
--   v_eco_author_identity
--
-- And it will seed the Bostrom + mk-bluebird identities.

----------------------------------------------------------------------
-- 2. Register eco_restoration_shard as a constellation repo
--
-- Goal: ensure this repo is present in the shared `repo` registry,
-- bound to mk-bluebird, and marked as a RESEARCH band repo.
--
-- PRECONDITION:
--   If the constellation already has the `repo` and `reporoleband`
--   tables (from the ecoconstellation index), reuse them.
--   Otherwise, create the minimal subset needed here.
-----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS reporoleband (
    roleband    TEXT PRIMARY KEY,  -- SPINE, RESEARCH, ENGINE, MATERIAL, GOV, APP
    description TEXT NOT NULL
);

INSERT OR IGNORE INTO reporoleband (roleband, description) VALUES
    ('RESEARCH', 'Non-actuating research and shard-generation workloads that feed planning.');

CREATE TABLE IF NOT EXISTS repo (
    repoid          INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL UNIQUE,   -- e.g. 'eco_restoration_shard'
    githubslug      TEXT NOT NULL,          -- e.g. 'mk-bluebird/eco_restoration_shard'
    visibility      TEXT NOT NULL CHECK (visibility IN ('Public','Private')),
    languageprimary TEXT NOT NULL,          -- 'Rust','C','Lua','Kotlin', etc.
    roleband        TEXT NOT NULL REFERENCES reporoleband(roleband),
    description     TEXT,
    lastupdatedutc  TEXT
);

-- ACTION 2.a: Insert eco_restoration_shard into the repo registry.
INSERT OR IGNORE INTO repo (
    name,
    githubslug,
    visibility,
    languageprimary,
    roleband,
    description,
    lastupdatedutc
) VALUES (
    'eco_restoration_shard',
    'mk-bluebird/eco_restoration_shard',
    'Public',
    'Rust',           -- primary language, adjust if different
    'RESEARCH',
    'Eco-restoration research shard emitter; non-actuating KER research for ecological restoration.',
    NULL              -- you can backfill with GitHub metadata later
);

----------------------------------------------------------------------
-- 3. Bind eco_restoration_shard to the Bostrom identity
--
-- Goal: connect the new mk-bluebird repo row to your canonical
-- eco_identity_person row seeded earlier, so all future shards
-- emitted from this repo can be attributed correctly without
-- referring to the legacy Doctor0Evil login.
-----------------------------------------------------------------------

-- ACTION 3.a: Create/ensure eco_repo_identity_binding exists (if not loaded).
CREATE TABLE IF NOT EXISTS eco_repo_identity_binding (
    binding_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_id          INTEGER NOT NULL,
    person_id        INTEGER NOT NULL REFERENCES eco_identity_person(person_id)
                         ON DELETE CASCADE,
    github_login     TEXT NOT NULL,
    is_current_owner INTEGER NOT NULL DEFAULT 1 CHECK (is_current_owner IN (0,1)),
    created_utc      TEXT NOT NULL DEFAULT (datetime('now')),
    updated_utc      TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(repo_id, github_login)
);

-- ACTION 3.b: Insert a binding row for eco_restoration_shard → mk-bluebird.
WITH me AS (
    SELECT person_id
    FROM eco_identity_person
    WHERE canonical_name = 'bostrom'
),
repo_row AS (
    SELECT repoid
    FROM repo
    WHERE name = 'eco_restoration_shard'
)
INSERT OR IGNORE INTO eco_repo_identity_binding (
    repo_id,
    person_id,
    github_login,
    is_current_owner
)
SELECT
    repo_row.repoid,
    me.person_id,
    'mk-bluebird',
    1
FROM me, repo_row;

----------------------------------------------------------------------
-- 4. Optional: register legacy Doctor0Evil evidence for this repo
--
-- Goal: if any eco_restoration_shard particles or shards were originally
-- authored under Doctor0Evil (e.g. imported from older repos), we want
-- a place to register that evidence so that governance and reward
-- engines can still attribute it to the same eco_identity_person.
-----------------------------------------------------------------------

-- Ensure eco_author_evidence exists (if the migration file was not loaded).
CREATE TABLE IF NOT EXISTS eco_author_evidence (
    evidence_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    person_id          INTEGER NOT NULL REFERENCES eco_identity_person(person_id)
                            ON DELETE CASCADE,
    repo_id            INTEGER,
    shard_id           INTEGER,
    schema_id          INTEGER,
    particle_id        INTEGER,
    source_kind        TEXT NOT NULL,
    source_locator     TEXT NOT NULL,
    legacy_github      TEXT NOT NULL,
    canonical_github   TEXT NOT NULL,
    primary_addr_id    INTEGER REFERENCES eco_identity_address(address_id)
                            ON DELETE SET NULL,
    primary_addr_value TEXT,
    signing_did        TEXT,
    created_utc        TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(source_kind, source_locator, legacy_github)
);

-- ACTION 4.a (manual for coder):
--   For each legacy artifact you migrate into this repo
--   (e.g., imported qpudatashards or ALN particles from older repos),
--   insert a row into eco_author_evidence using sqlite3 CLI.
--
-- Example shell command template (replace placeholders):
--   sqlite3 db/eco_constellation.db "
--   WITH me AS (
--     SELECT person_id FROM eco_identity_person WHERE canonical_name = 'bostrom'
--   ),
--   repo_row AS (
--     SELECT repoid FROM repo WHERE name = 'eco_restoration_shard'
--   )
--   INSERT OR IGNORE INTO eco_author_evidence (
--     person_id,
--     repo_id,
--     shard_id,
--     schema_id,
--     particle_id,
--     source_kind,
--     source_locator,
--     legacy_github,
--     canonical_github,
--     primary_addr_id,
--     primary_addr_value,
--     signing_did
--   )
--   SELECT
--     me.person_id,
--     repo_row.repoid,
--     NULL,
--     NULL,
--     NULL,
--     'GIT_COMMIT',
--     'Doctor0Evil/<repo-name>@<commit-hash>',
--     'Doctor0Evil',
--     'mk-bluebird',
--     (SELECT address_id
--      FROM eco_identity_address
--      WHERE person_id = me.person_id
--        AND is_primary = 1
--      LIMIT 1),
--     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
--     NULL
--   FROM me, repo_row;
--   "

----------------------------------------------------------------------
-- 5. Surface the constellation identity into eco_restoration_shard
--
-- Goal: provide a local view that eco_restoration_shard can query
-- to display “who owns this repo” and “which addresses sign shards.”
-----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_eco_restoration_identity AS
SELECT
    r.repoid,
    r.name          AS repo_name,
    r.githubslug,
    r.roleband,
    p.person_id,
    p.canonical_name,
    p.display_name,
    a.addr_kind,
    a.addr_value,
    a.is_primary,
    b.github_login,
    b.is_current_owner
FROM repo AS r
JOIN eco_repo_identity_binding AS b
    ON b.repo_id = r.repoid
JOIN eco_identity_person AS p
    ON p.person_id = b.person_id
LEFT JOIN eco_identity_address AS a
    ON a.person_id = p.person_id
WHERE r.name = 'eco_restoration_shard';

-- ACTION 5.a: verify the view from sqlite3 CLI:
--   sqlite3 db/eco_constellation.db "SELECT * FROM v_eco_restoration_identity;"

----------------------------------------------------------------------
-- 6. Coder-facing checklist (no Rust, only SQLite)
--
-- Use this order for the session:
--
-- 6.1 Clone repo and open DB:
--     git clone https://github.com/mk-bluebird/eco_restoration_shard.git
--     cd eco_restoration_shard
--     mkdir -p db
--     sqlite3 db/eco_constellation.db ".databases"
--
-- 6.2 Load eco identity spine:
--     sqlite3 db/eco_constellation.db ".read db/db_ecological_identity_migration.sql"
--
-- 6.3 Load this setup task file:
--     sqlite3 db/eco_constellation.db ".read db/tasks_eco_restoration_shard_setup.sql"
--
-- 6.4 Inspect registrations:
--     sqlite3 db/eco_constellation.db "
--       SELECT name, githubslug, roleband FROM repo
--       WHERE name = 'eco_restoration_shard';
--     "
--
--     sqlite3 db/eco_constellation.db "
--       SELECT * FROM v_eco_restoration_identity;
--     "
--
-- 6.5 For each imported legacy shard or particle, record evidence using
--     an INSERT into eco_author_evidence (see ACTION 4.a template).
--
-- 6.6 Commit db/* .sql files and updated DB (if checked in) back to
--     https://github.com/mk-bluebird/eco_restoration_shard so that
--     other constellation repos can attach this DB and share the same
--     migration spine without installing Rust.
