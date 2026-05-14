-- filename db_ecoconstellation_index.sql
-- destination eco_restoration_shard/db/db_ecoconstellation_index.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Role band table (SPINE / RESEARCH / ENGINE / MATERIAL / GOV / APP)
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS reporoleband (
    roleband    TEXT PRIMARY KEY,          -- SPINE,RESEARCH,ENGINE,MATERIAL,GOV,APP
    description TEXT NOT NULL
);

INSERT OR IGNORE INTO reporoleband (roleband, description) VALUES
    ('SPINE',   'Core ecosafety grammar, ALN schemas, qpudatashard invariants, and tooling'),
    ('RESEARCH','Non-actuating research and shard-generation workloads that feed planning'),
    ('ENGINE',  'Physical-domain kernels and controllers, fenced by ecosafety spine'),
    ('MATERIAL','Material and biology repositories for substrates, species, and corridors'),
    ('GOV',     'Governance, finance, rights, orchestration, and reward logic'),
    ('APP',     'Specialized deployments, clients, dashboards, and city-specific bridges');

-------------------------------------------------------------------------------
-- 2. Canonical repo registry
--    This instance treats eco_restoration_shard as the canonical index host.
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS repo (
    repoid         INTEGER PRIMARY KEY AUTOINCREMENT,
    name           TEXT NOT NULL UNIQUE,   -- e.g. Eco-Fort, eco_restoration_shard
    githubslug     TEXT NOT NULL,          -- e.g. mk-bluebird/eco_restoration_shard
    visibility     TEXT NOT NULL CHECK (visibility IN ('Public','Private')),
    languageprimary TEXT NOT NULL,         -- Rust,C,CPP,Lua,Kotlin,Java,CSV,ALN, etc.
    roleband       TEXT NOT NULL REFERENCES reporoleband(roleband),
    description    TEXT,
    successor_repo TEXT,                   -- optional: canonical successor for deprecated repos
    lastupdatedutc TEXT                    -- ISO8601 from CI or GitHub metadata
);

CREATE INDEX IF NOT EXISTS idx_repo_roleband    ON repo(roleband);
CREATE INDEX IF NOT EXISTS idx_repo_visibility  ON repo(visibility);
CREATE INDEX IF NOT EXISTS idx_repo_successor   ON repo(successor_repo);

-------------------------------------------------------------------------------
-- 3. Seed data – mk-bluebird constellation (partial, extend as needed)
--    NOTE: lastupdatedutc is left NULL for CI/backfill; no fabricated timestamps.
-------------------------------------------------------------------------------

INSERT OR IGNORE INTO repo
(name, githubslug, visibility, languageprimary, roleband, description, successor_repo, lastupdatedutc)
VALUES
    -- Canonical index host for this constellation shard
    ('eco_restoration_shard',
     'mk-bluebird/eco_restoration_shard',
     'Public',
     'Rust',
     'RESEARCH',
     'Eco-restoration shard emitter and constellation index host for mk-bluebird, focused on carbon-negative, restorative workloads and KER-tightening research.',
     NULL,
     NULL),

    -- Ecological-Order orchestrator (GOV band)
    ('Ecological-Order',
     'mk-bluebird/Ecological-Order',
     'Public',
     'Rust',
     'GOV',
     'EcoNet constellation orchestrator indexing and mapping repos, schemas, and shards for sovereign eco-governance across smart-city workloads.',
     NULL,
     NULL),

    -- Eco-Fort mirror or future mk-bluebird fork (SPINE)
    ('Eco-Fort',
     'mk-bluebird/Eco-Fort',
     'Public',
     'Rust',
     'SPINE',
     'Centralized repository of data schemas, validation rules, and qpudatashard references for ecosafety governance under mk-bluebird.',
     NULL,
     NULL),

    -- aln-platform-ecosystem under mk-bluebird (SPINE)
    ('aln-platform-ecosystem',
     'mk-bluebird/aln-platform-ecosystem',
     'Public',
     'Rust',
     'SPINE',
     'Canonical ALN spec library (ecosafety.riskvector, ecosafety.corridors, shard schemas) for mk-bluebird constellation.',
     NULL,
     NULL),

    -- Example ENGINE repo: Phoenix water kernels (if recreated under mk-bluebird)
    ('EcoNet-CEIM-PhoenixWater',
     'mk-bluebird/EcoNet-CEIM-PhoenixWater',
     'Public',
     'Rust',
     'ENGINE',
     'CEIM/CPVM kernels and controllers for water nodes (PFAS, E. coli, salinity) under ecosafety constraints.',
     NULL,
     NULL),

    -- Example MATERIAL repo: BugsLife
    ('BugsLife',
     'mk-bluebird/BugsLife',
     'Public',
     'Rust',
     'MATERIAL',
     'Eco-friendly pest-control substrates and tactics that avoid hazardous chemicals and protect ecosystems.',
     NULL,
     NULL),

    -- Example GOV repo: Paycomp
    ('Paycomp',
     'mk-bluebird/Paycomp',
     'Public',
     'Rust',
     'GOV',
     'Augmented-citizen financial infrastructure for smart-city payments linked to eco-impact and KER.',
     NULL,
     NULL);

-- Add more repos as you reconstitute them under mk-bluebird.
-- For defunct Doctor0Evil repos, insert a row with githubslug = 'Doctor0Evil/...'
-- and successor_repo = 'eco_restoration_shard' or another mk-bluebird replacement.
