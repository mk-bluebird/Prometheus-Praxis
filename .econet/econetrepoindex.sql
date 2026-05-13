-- filename: .econet/econetrepoindex.sql
-- Purpose: Master-index SQL shard for eco_response_shard under the mk-bluebird spine.
-- Guides AI/chat/coding agents on repo role, layers, and KER targets.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS econetrepoindex (
    reponame         TEXT PRIMARY KEY,
    githubslug       TEXT NOT NULL,
    roleband         TEXT NOT NULL,           -- SPINE, RESEARCH, ENGINE, MATERIAL, GOV, APP
    visibility       TEXT NOT NULL,           -- Public, Private
    languageprimary  TEXT NOT NULL,           -- Rust for eco_response_shard
    description      TEXT,
    ecosafetybinding TEXT NOT NULL,           -- frozen ecosafety grammar, e.g. cyboquatic-ecosafety-core2026v1
    shardprotocol    TEXT NOT NULL,           -- e.g. EcoNetSchemaShard2026v1
    lanedefault      TEXT NOT NULL,           -- RESEARCH, EXPPROD, PROD
    kertargetk       REAL NOT NULL,
    kertargete       REAL NOT NULL,
    kertargetr       REAL NOT NULL,
    nonactuatingonly INTEGER NOT NULL CHECK (nonactuatingonly IN (0,1)),
    didowner         TEXT NOT NULL,           -- Bostrom DID of repo owner
    evidencehex      TEXT                     -- hex-stamp of this manifest row
);

CREATE TABLE IF NOT EXISTS econetlayer (
    layerid     INTEGER PRIMARY KEY AUTOINCREMENT,
    reponame    TEXT NOT NULL REFERENCES econetrepoindex(reponame) ON DELETE CASCADE,
    layername   TEXT NOT NULL,               -- e.g. RESPONSE_INDEX, RESEARCH_SHARDS
    layertier   TEXT NOT NULL,               -- GRAMMARCLIENT, INDEX, RESEARCH, GOV, OTHER
    languages   TEXT NOT NULL,               -- comma-separated: Rust
    description TEXT,
    contracts   TEXT                         -- invariants: "NonActuatingWorkload; NoCorridorNoBuild"
);

CREATE TABLE IF NOT EXISTS econetrolehint (
    hintid   INTEGER PRIMARY KEY AUTOINCREMENT,
    reponame TEXT NOT NULL REFERENCES econetrepoindex(reponame) ON DELETE CASCADE,
    key      TEXT NOT NULL,                  -- e.g. "shardtypes", "primaryparticles", "pilotdomains"
    value    TEXT NOT NULL
);

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
    didowner,
    evidencehex
) VALUES (
    'eco_response_shard',
    'mk-bluebird/eco_response_shard',
    'RESEARCH',
    'Public',
    'Rust',
    'Non-actuating response-index repo that mirrors KER, Lyapunov residuals, and corridors from the econet spine for governance proofs and eco-restoration research.',
    'cyboquatic-ecosafety-core2026v1.aln',
    'EcoNetSchemaShard2026v1',
    'RESEARCH',
    0.95,
    0.91,
    0.12,
    1,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    '0xRESPONSESPECHEX2026ABCD'
);

INSERT INTO econetlayer (
    reponame,
    layername,
    layertier,
    languages,
    description,
    contracts
) VALUES
(
    'eco_response_shard',
    'ResponseShardIndex',
    'INDEX',
    'Rust',
    'Backfill and index layer that reads econet-index SQLite, projects shardinstance/knowledgeecoscore into response_shard, and never actuates hardware.',
    'NonActuatingWorkload; NoFFIActuators; NoCorridorWeakening'
),
(
    'eco_response_shard',
    'ResearchExports',
    'RESEARCH',
    'Rust',
    'Research export layer that writes qpudatashards and ALN particles (ResponseShardEcoMetrics2026v1) for downstream agents and dashboards.',
    'NonActuatingWorkload; NoFFIActuators; NoCorridorWeakening'
);

INSERT INTO econetrolehint (reponame, key, value) VALUES
('eco_response_shard', 'shardtypes', 'ResponseShardEcoMetrics2026v1;ResponseBackfillMeta2026v1'),
('eco_response_shard', 'primaryparticles', 'ResponseShardEcoMetrics2026v1'),
('eco_response_shard', 'pilotdomains', 'EcoRestorationGovernance;PhoenixWater;BiodegradableMaterials'),
('eco_response_shard', 'spinebinding', 'econet-index.sqlite:shardinstance;knowledgeecoscore;corridordefinition');
