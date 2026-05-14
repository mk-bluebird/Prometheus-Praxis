-- filename: tasks_eco_restoration_shard_next_steps.sql
-- destination: eco_restoration_shard/db/tasks_eco_restoration_shard_next_steps.sql
--
-- NOTE TO CODER:
-- - Continue to avoid installing Rustup/Cargo.
-- - Use only sqlite3 and shell tools.
-- - These tasks assume eco_constellation.db already contains:
--     * eco_identity_* and eco_github_* tables with Bostrom/mk-bluebird
--     * repo + reporoleband with eco_restoration_shard as RESEARCH
--     * eco_repo_identity_binding + v_eco_restoration_identity
-- - All actions below remain non-actuating and governance-only.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Create a per-repo master index shard (.econet-eco-repo-index.sql)
--    for eco_restoration_shard, using the shared econetrepoindex schema.
--
-- Goal:
-- - Give eco_restoration_shard a self-describing manifest that agents
--   and CI can read to understand roleband, nonactuating status,
--   lane defaults, and KER targets.
--
-- ACTION 1.a (schema): add econetrepoindex + econetlayer tables
--                     into eco_constellation.db if not present.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS econetrepoindex (
    reponame         TEXT PRIMARY KEY,
    githubslug       TEXT NOT NULL,
    roleband         TEXT NOT NULL,  -- SPINE, RESEARCH, ENGINE, MATERIAL, GOV, APP
    visibility       TEXT NOT NULL,  -- Public, Private
    languageprimary  TEXT NOT NULL,
    description      TEXT,
    ecosafetybinding TEXT NOT NULL,  -- e.g. 'EcosafetyGrammar2026v1.aln'
    shardprotocol    TEXT NOT NULL,  -- e.g. 'EcoNetSchemaShard2026v1'
    lanedefault      TEXT NOT NULL,  -- RESEARCH, EXPPROD, PROD
    kertargetk       REAL NOT NULL,
    kertargete       REAL NOT NULL,
    kertargetr       REAL NOT NULL,
    nonactuatingonly INTEGER NOT NULL CHECK (nonactuatingonly IN (0,1))
);

CREATE TABLE IF NOT EXISTS econetlayer (
    layerid     INTEGER PRIMARY KEY AUTOINCREMENT,
    reponame    TEXT NOT NULL REFERENCES econetrepoindex(reponame)
                    ON DELETE CASCADE,
    layername   TEXT NOT NULL,
    layertier   TEXT NOT NULL,   -- GRAMMAR, KERNEL, EDGESCRIPT, UI, GOVERNANCE, MATERIAL, OTHER
    languages   TEXT NOT NULL,   -- e.g. 'Rust,C', 'CPP,Lua'
    description TEXT,
    contracts   TEXT             -- e.g. 'NonActuatingWorkload,SafeKernel'
);

CREATE TABLE IF NOT EXISTS econetlayerlanepolicy (
    policyid    INTEGER PRIMARY KEY AUTOINCREMENT,
    reponame    TEXT NOT NULL REFERENCES econetrepoindex(reponame)
                    ON DELETE CASCADE,
    layername   TEXT NOT NULL,
    laneallowed TEXT NOT NULL,   -- RESEARCH, EXPPROD, PROD
    kermink     REAL,
    kermine     REAL,
    kermaxr     REAL
);

CREATE TABLE IF NOT EXISTS econetrolehint (
    hintid    INTEGER PRIMARY KEY AUTOINCREMENT,
    reponame  TEXT NOT NULL REFERENCES econetrepoindex(reponame)
                 ON DELETE CASCADE,
    key       TEXT NOT NULL,
    value     TEXT NOT NULL
);

----------------------------------------------------------------------
-- ACTION 1.b (data): register eco_restoration_shard in econetrepoindex
--                    and describe its internal layers.
--
-- Assumptions:
-- - eco_restoration_shard is RESEARCH, Public, Rust-first.
-- - It is strictly non-actuating (research-only).
-- - Use KER targets aligned with RESEARCH band (looser than PROD).
----------------------------------------------------------------------

INSERT OR REPLACE INTO econetrepoindex (
    reponame,
    githubslug,
    roleband,
    visibility,
    languageprimary,
    description,
    ecosafetybinding,
    shardprotocol,
    lanedefault,
    kertargetk,
    kertargete,
    kertargetr,
    nonactuatingonly
) VALUES (
    'eco_restoration_shard',
    'mk-bluebird/eco_restoration_shard',
    'RESEARCH',
    'Public',
    'Rust',
    'Eco-restoration research shard emitter; non-actuating KER and corridor research for ecological restoration.',
    'EcosafetyGrammar2026v1.aln',
    'EcoNetSchemaShard2026v1',
    'RESEARCH',
    0.90,
    0.90,
    0.20,
    1
);

-- Describe layers (you can refine these later as the repo evolves).
DELETE FROM econetlayer WHERE reponame = 'eco_restoration_shard';

INSERT INTO econetlayer (
    reponame, layername, layertier, languages, description, contracts
) VALUES
    (
        'eco_restoration_shard',
        'ALN grammar and shard specs',
        'GRAMMAR',
        'Rust',
        'Defines ALN schemas and qpudatashard formats for eco-restoration research.',
        'NonActuatingWorkload'
    ),
    (
        'eco_restoration_shard',
        'Shard generator kernels',
        'KERNEL',
        'Rust',
        'Non-actuating kernels that compute KER metrics and risk coordinates from evidence and telemetry.',
        'NonActuatingWorkload,SafeKernel'
    );

DELETE FROM econetlayerlanepolicy WHERE reponame = 'eco_restoration_shard';

INSERT INTO econetlayerlanepolicy (
    reponame, layername, laneallowed, kermink, kermine, kermaxr
) VALUES
    (
        'eco_restoration_shard',
        'ALN grammar and shard specs',
        'RESEARCH',
        0.80,
        0.80,
        0.30
    ),
    (
        'eco_restoration_shard',
        'Shard generator kernels',
        'RESEARCH',
        0.85,
        0.85,
        0.25
    );

DELETE FROM econetrolehint WHERE reponame = 'eco_restoration_shard';

INSERT INTO econetrolehint (
    reponame, key, value
) VALUES
    ('eco_restoration_shard', 'primaryplane', 'biodiversity'),
    ('eco_restoration_shard', 'primaryplane2', 'carbon'),
    ('eco_restoration_shard', 'regionfocus', 'Phoenix-AZ'),
    ('eco_restoration_shard', 'band', 'RESEARCH'),
    ('eco_restoration_shard', 'nonactuating', 'true');

----------------------------------------------------------------------
-- 2. Wire eco_restoration_shard into shardinstance and knowledgeecoscore
--
-- Goal:
-- - Prepare this repo to log KER-scored research shards in a way that
--   EcoNet, Eco-Fort, ecological-orchestrator, and Paycomp can consume.
-- - All still non-actuating: we only register shard metadata and scores.
--
-- If shardinstance / knowledgeecoscore already exist (from earlier
-- constellation spine work), this section will be a no-op. Otherwise,
-- it creates minimal compatible versions for this repo.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS shardinstance (
    shardid       INTEGER PRIMARY KEY AUTOINCREMENT,
    repofileid    INTEGER,   -- can be NULL here if repofile is not yet populated
    particleid    INTEGER,
    nodeid        TEXT,
    assettype     TEXT,
    medium        TEXT,
    region        TEXT,
    tstartutc     TEXT,
    tendutc       TEXT,
    lane          TEXT,      -- RESEARCH, EXPPROD, PROD
    kmetric       REAL,
    emetric       REAL,
    rmetric       REAL,
    vtmax         REAL,
    kerdeployable INTEGER NOT NULL DEFAULT 0 CHECK (kerdeployable IN (0,1)),
    evidencehex   TEXT,
    signingdid    TEXT
);

CREATE INDEX IF NOT EXISTS idx_shard_node_time
    ON shardinstance (nodeid, tstartutc, tendutc);

CREATE INDEX IF NOT EXISTS idx_shard_lane_ker
    ON shardinstance (lane, kerdeployable, emetric, rmetric);

CREATE INDEX IF NOT EXISTS idx_shard_region
    ON shardinstance (region);

CREATE TABLE IF NOT EXISTS knowledgeecoscore (
    scoreid        INTEGER PRIMARY KEY AUTOINCREMENT,
    scopetype      TEXT NOT NULL
                   CHECK (scopetype IN ('REPO','FILE','SCHEMA','PARTICLE','SHARD','DOCUMENT')),
    scoperefid     INTEGER NOT NULL,
    kfactor        REAL NOT NULL,
    efactor        REAL NOT NULL,
    rfactor        REAL NOT NULL,
    rationale      TEXT NOT NULL,
    timestamputc   TEXT NOT NULL,
    issuedby       TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_kerscore_scope
    ON knowledgeecoscore (scopetype, scoperefid);

----------------------------------------------------------------------
-- ACTION 2.a: Seed a REPO-level knowledgeecoscore entry for
--             eco_restoration_shard so Paycomp / EcoNet-CERG can
--             see that this repo is an eco-helpful research source.
----------------------------------------------------------------------

WITH repo_row AS (
    SELECT repoid
    FROM repo
    WHERE name = 'eco_restoration_shard'
)
INSERT OR IGNORE INTO knowledgeecoscore (
    scopetype,
    scoperefid,
    kfactor,
    efactor,
    rfactor,
    rationale,
    timestamputc,
    issuedby
)
SELECT
    'REPO',
    repo_row.repoid,
    0.93,
    0.90,
    0.15,
    'Non-actuating eco-restoration research repository; KER scores reflect research-only shards and strict ecosafety grammar use.',
    datetime('now'),
    'bostrom'
FROM repo_row;

----------------------------------------------------------------------
-- 3. Define a local view for eco_restoration_shard research shards
--
-- Goal:
-- - Give agents a one-stop view that joins repo metadata, identity,
--   shardinstance KER metrics, and knowledgeecoscore for this repo.
----------------------------------------------------------------------

DROP VIEW IF EXISTS v_eco_restoration_shard_research;

CREATE VIEW v_eco_restoration_shard_research AS
SELECT
    s.shardid,
    s.nodeid,
    s.assettype,
    s.medium,
    s.region,
    s.tstartutc,
    s.tendutc,
    s.lane,
    s.kmetric,
    s.emetric,
    s.rmetric,
    s.vtmax,
    s.kerdeployable,
    s.evidencehex,
    s.signingdid,
    r.repoid,
    r.name            AS repo_name,
    r.githubslug,
    r.roleband,
    p.person_id,
    p.canonical_name,
    p.display_name,
    a.addr_value      AS signing_addr,
    kes.kfactor       AS repo_kfactor,
    kes.efactor       AS repo_efactor,
    kes.rfactor       AS repo_rfactor
FROM shardinstance AS s
JOIN repo AS r
    ON r.name = 'eco_restoration_shard'
LEFT JOIN eco_repo_identity_binding AS b
    ON b.repo_id = r.repoid
LEFT JOIN eco_identity_person AS p
    ON p.person_id = b.person_id
LEFT JOIN eco_identity_address AS a
    ON a.person_id = p.person_id
   AND a.is_primary = 1
LEFT JOIN knowledgeecoscore AS kes
    ON kes.scopetype = 'REPO'
   AND kes.scoperefid = r.repoid;

----------------------------------------------------------------------
-- 4. Coder checklist (shell + sqlite3 only)
--
-- Run these commands manually in your /workspace/eco_restoration_shard:
--
-- 4.1 Load the new tasks file:
--     sqlite3 db/eco_constellation.db \
--       ".read db/tasks_eco_restoration_shard_next_steps.sql"
--
-- 4.2 Verify econetrepoindex + econetlayer entries:
--     sqlite3 db/eco_constellation.db "
--       SELECT reponame, roleband, lanedefault, nonactuatingonly
--       FROM econetrepoindex
--       WHERE reponame = 'eco_restoration_shard';
--     "
--
--     sqlite3 db/eco_constellation.db "
--       SELECT layername, layertier, languages, contracts
--       FROM econetlayer
--       WHERE reponame = 'eco_restoration_shard';
--     "
--
-- 4.3 Verify knowledgeecoscore for this repo:
--     sqlite3 db/eco_constellation.db "
--       SELECT scopetype, kfactor, efactor, rfactor, issuedby
--       FROM knowledgeecoscore
--       WHERE scopetype = 'REPO'
--         AND scoperefid = (
--           SELECT repoid FROM repo
--           WHERE name = 'eco_restoration_shard'
--         );
--     "
--
-- 4.4 (Optional, when shards exist) Insert a test RESEARCH shard:
--     sqlite3 db/eco_constellation.db "
--       INSERT INTO shardinstance (
--         nodeid, assettype, medium, region,
--         tstartutc, tendutc,
--         lane, kmetric, emetric, rmetric,
--         vtmax, kerdeployable, evidencehex, signingdid
--       ) VALUES (
--         'PHX-RESEARCH-NODE-01',
--         'EcoResearchNode',
--         'water',
--         'Phoenix-AZ',
--         '2026-01-01T00:00:00Z',
--         '2026-01-31T23:59:59Z',
--         'RESEARCH',
--         0.92, 0.88, 0.18,
--         0.25,
--         0,
--         'deadbeefcafebabe0011223344556677',
--         'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
--       );
--     "
--
--     sqlite3 db/eco_constellation.db "
--       SELECT shardid, nodeid, kmetric, emetric, rmetric,
--              canonical_name, signing_addr
--       FROM v_eco_restoration_shard_research
--       ORDER BY shardid DESC
--       LIMIT 5;
--     "
--
-- 4.5 Commit the updated db/*.sql and, if you track it, the
--     db/eco_constellation.db file to:
--       https://github.com/mk-bluebird/eco_restoration_shard
--
--     This will allow other constellation repos to attach this DB
--     and discover eco_restoration_shard’s role, KER scores, and
--     identity mappings without any Rust/Cargo tooling.
