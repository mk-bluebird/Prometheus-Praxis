-- filename: .econet/econet_repoindex.sql
-- Purpose: Master index SQL-shard for EcoNet constellation repos.
-- Guides AI and coding agents on repo roles, layers, and ecosafety invariants.

PRAGMA foreign_keys = ON;

-- Canonical per-repo index (normalized, enriched version)
CREATE TABLE IF NOT EXISTS econetrepoindex (
    reponame          TEXT PRIMARY KEY,
    githubslug        TEXT NOT NULL,
    roleband          TEXT NOT NULL,  -- SPINE,RESEARCH,ENGINE,MATERIAL,GOV,APP
    visibility        TEXT NOT NULL,  -- Public,Private
    languageprimary   TEXT NOT NULL,
    description       TEXT,
    ecosafetybinding  TEXT NOT NULL,  -- e.g. cyboquatic-ecosafety-coreEcosafetyGrammar2026v1.aln
    shardprotocol     TEXT NOT NULL,  -- e.g. ALN-RFC4180EcoNetSchemaShard2026v1
    lanedefault       TEXT NOT NULL,  -- RESEARCH,EXPPROD,PROD
    kertargetk        REAL NOT NULL,
    kertargete        REAL NOT NULL,
    kertargetr        REAL NOT NULL,
    nonactuatingonly  INTEGER NOT NULL CHECK (nonactuatingonly IN (0,1)),
    signingdid        TEXT,           -- optional Bostrom DID for repo manifest
    evidencehex       TEXT            -- optional hexstamp of manifest content
);

CREATE INDEX IF NOT EXISTS idx_econetrepoindex_roleband
    ON econetrepoindex (roleband);

CREATE INDEX IF NOT EXISTS idx_econetrepoindex_visibility
    ON econetrepoindex (visibility);

-- Per-repo programming layers
CREATE TABLE IF NOT EXISTS econetlayer (
    layerid     INTEGER PRIMARY KEY AUTOINCREMENT,
    reponame    TEXT NOT NULL REFERENCES econetrepoindex(reponame) ON DELETE CASCADE,
    layername   TEXT NOT NULL,
    layertier   TEXT NOT NULL,  -- GRAMMAR,KERNEL,EDGESCRIPT,UI,GOVERNANCE,MATERIAL,OTHER
    languages   TEXT NOT NULL,  -- comma-separated list
    description TEXT,
    contracts   TEXT            -- human-readable invariants, e.g. "SafeKernel,NonActuatingWorkload"
);

CREATE INDEX IF NOT EXISTS idx_econetlayer_repo
    ON econetlayer (reponame);

CREATE INDEX IF NOT EXISTS idx_econetlayer_tier
    ON econetlayer (layertier);

-- Optional free-form hints for agents (shard types, primary planes, pilot domains, etc.)
CREATE TABLE IF NOT EXISTS econetrolehint (
    hintid   INTEGER PRIMARY KEY AUTOINCREMENT,
    reponame TEXT NOT NULL REFERENCES econetrepoindex(reponame) ON DELETE CASCADE,
    key      TEXT NOT NULL,
    value    TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_econetrolehint_repo
    ON econetrolehint (reponame, key);

-- Seed manifest for EcoNet-CEIM-PhoenixWater (ENGINE band cyboquatic repo)
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
    nonactuatingonly,
    signingdid,
    evidencehex
) VALUES (
    'EcoNet-CEIM-PhoenixWater',
    'mk-bluebird/EcoNet-CEIM-PhoenixWater',
    'ENGINE',
    'Public',
    'Rust',
    'Cyboquatic CEIM/CPVM kernels and ecosafety-gated routing for Phoenix water nodes.',
    'cyboquatic-ecosafety-coreEcosafetyGrammar2026v1.aln',
    'ALN-RFC4180EcoNetSchemaShard2026v1',
    'EXPPROD',
    0.94,
    0.90,
    0.13,
    0,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    NULL
);

INSERT INTO econetlayer (
    reponame, layername, layertier, languages, description, contracts
) VALUES
(
    'EcoNet-CEIM-PhoenixWater',
    'Ecosafety spine client',
    'GRAMMAR',
    'Rust',
    'Imports RiskCoord, RiskVector, residual, and KER from cyboquatic ecosafety core.',
    'Must not redefine residual or KER math; V(t+1) <= V(t) for all replayed workloads.'
),
(
    'EcoNet-CEIM-PhoenixWater',
    'Hydrology kernels',
    'KERNEL',
    'Rust,C',
    'Implements CEIM mass-load and CPVM viability kernels for Phoenix hydrological basins.',
    'Non-actuating: kernels compute rx, Vt, and KER only; actuation handled by fenced gateway crates.'
),
(
    'EcoNet-CEIM-PhoenixWater',
    'Hydrological buffer atlas',
    'KERNEL',
    'Rust',
    'Maintains rFOG, rTDS, rEcoli, and Vt for hydrological reaches using qpudatashards.',
    'Outputs tagged by lane; corridors may tighten but never loosen hazard bands.'
);

INSERT INTO econetrolehint (reponame, key, value) VALUES
('EcoNet-CEIM-PhoenixWater', 'shardtypes', 'HydrologicalBufferShard, PhoenixMarShard, FOGRoutingDecision'),
('EcoNet-CEIM-PhoenixWater', 'primaryplane', 'hydraulics'),
('EcoNet-CEIM-PhoenixWater', 'pilotdomains', 'Phoenix-AZ; Gila River; Lake Pleasant PFBS');
