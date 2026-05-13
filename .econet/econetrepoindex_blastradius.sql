-- filename: .econet/econetrepoindex_blastradius.sql
-- destination: EcoNet/.econet/econetrepoindex_blastradius.sql
-- purpose:
--   Repo-local master-index shard for the EcoNet blastradius spine crate.
--   Guides AI/coding agents on layers, languages, and invariants.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS econetrepoindex (
    reponame         TEXT PRIMARY KEY,
    githubslug       TEXT NOT NULL,
    roleband         TEXT NOT NULL,
    visibility       TEXT NOT NULL,
    languageprimary  TEXT NOT NULL,
    description      TEXT,
    ecosafetybinding TEXT NOT NULL,
    shardprotocol    TEXT NOT NULL,
    lanedefault      TEXT NOT NULL,
    kertargetk       REAL NOT NULL,
    kertargete       REAL NOT NULL,
    kertargetr       REAL NOT NULL,
    nonactuatingonly INTEGER NOT NULL CHECK (nonactuatingonly IN (0,1))
);

CREATE TABLE IF NOT EXISTS econetlayer (
    layerid     INTEGER PRIMARY KEY AUTOINCREMENT,
    reponame    TEXT NOT NULL REFERENCES econetrepoindex(reponame) ON DELETE CASCADE,
    layername   TEXT NOT NULL,
    layertier   TEXT NOT NULL,
    languages   TEXT NOT NULL,
    description TEXT,
    contracts   TEXT
);

CREATE TABLE IF NOT EXISTS econetrolehint (
    hintid   INTEGER PRIMARY KEY AUTOINCREMENT,
    reponame TEXT NOT NULL REFERENCES econetrepoindex(reponame) ON DELETE CASCADE,
    key      TEXT NOT NULL,
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
    nonactuatingonly
) VALUES (
    'EcoNet-blastradius-spine',
    'Doctor0Evil/EcoNet',
    'SPINE',
    'Public',
    'Rust',
    'Central EcoNet SQLite spine for cyboquatic blast-radius, canal, and eco-metric views; non-actuating diagnostics only.',
    'cyboquatic-ecosafety-coreEcosafetyGrammar2026v1.aln',
    'ALN-RFC4180EcoNetSchemaShard2026v1',
    'PROD',
    0.95,
    0.92,
    0.12,
    1
);

INSERT INTO econetlayer (
    reponame,
    layername,
    layertier,
    languages,
    description,
    contracts
) VALUES (
    'EcoNet-blastradius-spine',
    'Blast-radius SQLite spine',
    'GRAMMAR',
    'SQL,Rust',
    'Defines db/blastradius_spine.sql: blastradius_object, adjacency_graph, shard_ecoscore, eco_always_improve_policy, and eco/broadcast views.',
    'Non-actuating diagnostics only. No actuator FFI. All fields downstream of shardinstance and ecosafety shards; Vt and KER semantics imported, not redefined.'
);

INSERT INTO econetlayer (
    reponame,
    layername,
    layertier,
    languages,
    description,
    contracts
) VALUES (
    'EcoNet-blastradius-spine',
    'Edge-agent discovery layer',
    'EDGESCRIPT',
    'Rust,Lua,Kotlin,C++',
    'Provides read-only queries over v_shard_eco_blast and v_shard_eco_improvement for FOG routers and siting agents.',
    'All edge scripts must treat views as advisory. Any control decisions must pass through Rust/ALN ecosafety spine and lane governance.'
);

INSERT INTO econetrolehint (reponame, key, value) VALUES
('EcoNet-blastradius-spine', 'shardtypes', 'HydrologicalBufferShard,FlowVacSubstrateShard,FOGRoutingDecision'),
('EcoNet-blastradius-spine', 'primaryparticles', 'HydrologicalBufferPhoenix2026v1,FlowVacSubstrateShard.v1'),
('EcoNet-blastradius-spine', 'pilotdomains', 'Central-AZ hydrological buffers,Gila E. coli buffering,Lake Pleasant PFBS monitoring');
