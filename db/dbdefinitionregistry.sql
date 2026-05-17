-- filename: dbdefinitionregistry.sql
-- destination: eco_restoration_shard/db/dbdefinitionregistry.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 0. Contracts for frozen definitions
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS definitioncontract (
    contractid       TEXT PRIMARY KEY,
    scope            TEXT NOT NULL,
    registryversion  TEXT NOT NULL,
    description      TEXT NOT NULL,
    createdutc       TEXT NOT NULL,
    updatedutc       TEXT NOT NULL
);

-------------------------------------------------------------------------------
-- 1. Canonical mapping from logical definitions to concrete artifacts
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS definitionregistry (
    definitionid     INTEGER PRIMARY KEY AUTOINCREMENT,
    contractid       TEXT NOT NULL REFERENCES definitioncontract(contractid) ON DELETE CASCADE,
    scope            TEXT NOT NULL,
    logicalname      TEXT NOT NULL,
    kind             TEXT NOT NULL,
    repo             TEXT NOT NULL,
    destinationpath  TEXT NOT NULL,
    filename         TEXT NOT NULL,
    language         TEXT NOT NULL,
    versiontag       TEXT NOT NULL,
    active           INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    primaryplane     TEXT NOT NULL,
    appliescope      TEXT NOT NULL,
    summary          TEXT NOT NULL,
    signingdid       TEXT NOT NULL,
    issuedutc        TEXT NOT NULL,
    updatedutc       TEXT NOT NULL,
    UNIQUE (logicalname, versiontag),
    UNIQUE (repo, destinationpath, filename, versiontag)
);

CREATE INDEX IF NOT EXISTS idx_definition_scope_active
    ON definitionregistry (scope, active);

CREATE INDEX IF NOT EXISTS idx_definition_repo_active
    ON definitionregistry (repo, active);

CREATE INDEX IF NOT EXISTS idx_definition_contract
    ON definitionregistry (contractid, active);

CREATE INDEX IF NOT EXISTS idx_definition_kind_language
    ON definitionregistry (kind, language);

-------------------------------------------------------------------------------
-- 2. Seed contracts for ecosafety, eco-wealth, and eco-ledger surfaces
-------------------------------------------------------------------------------

INSERT OR IGNORE INTO definitioncontract (
    contractid,                 scope,             registryversion,
    description,                createdutc,        updatedutc
) VALUES
    -- Ecosafety grammar and residual math
    ('EcosafetyContinuity2026v1', 'ECOSAFETYCORE', '2026v1',
     'Frozen ecosafety grammar (planes, risk coordinates, corridors, residual kernel, KER gates).',
     '2026-05-03T07:15:00Z', '2026-05-03T07:15:00Z'),

    -- Plane weights and non-offsettable bands
    ('EcosafetyPlaneWeights2026v1', 'PLANEWEIGHTS', '2026v1',
     'Plane weights, non-compensation invariants, topology plane wiring.',
     '2026-05-03T07:15:00Z', '2026-05-03T07:15:00Z'),

    -- Lane governance and lane status shards
    ('LaneGovernance2026v1', 'LANEGOVERNANCE', '2026v1',
     'Lane predicates, lane status shards, lane verdicts, and CI gates.',
     '2026-05-03T07:15:00Z', '2026-05-03T07:15:00Z'),

    -- Topology risk plane
    ('TopologyRisk2026v1', 'TOPOLOGYRISK', '2026v1',
     'Topology audit, Itopology / rtopology metrics, and governance drift tracking.',
     '2026-05-03T07:15:00Z', '2026-05-03T07:15:00Z'),

    -- Blast radius and adjacency
    ('BlastRadius2026v1', 'BLASTRADIUS', '2026v1',
     'Blast radius, adjacency graph, and tbr2026v1 hex descriptors.',
     '2026-05-03T07:15:00Z', '2026-05-03T07:15:00Z'),

    -- Artifact registry and provenance
    ('ArtifactRegistry2026v1', 'ARTIFACTREGISTRY', '2026v1',
     'Universal artifact registry and provenance chain for governed artifacts.',
     '2026-05-03T07:15:00Z', '2026-05-03T07:15:00Z'),

    -- Steward eco-wealth statements (this shard)
    ('EcoWealthStewardStatement2026v1', 'ECOWEALTH', '2026v1',
     'Steward eco-wealth statements per region and KER context.',
     '2026-05-17T07:15:00Z', '2026-05-17T07:15:00Z'),

    -- Eco-wealth math views (shard residual and eco-wealth view)
    ('EcoWealthMathSpine2026v1', 'ECOWEALTHMATH', '2026v1',
     'Residual views and eco-wealth views for KER-backed eco-wealth.',
     '2026-05-17T07:15:00Z', '2026-05-17T07:15:00Z');

-------------------------------------------------------------------------------
-- 3. Seed entries for ecosafety core and artifact registry
-------------------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry (
    contractid, scope, logicalname, kind,
    repo, destinationpath, filename, language,
    versiontag, active, primaryplane, appliescope,
    summary, signingdid, issuedutc, updatedutc
) VALUES
    -- Ecosafety core grammar
    ('EcosafetyContinuity2026v1', 'ECOSAFETYCORE',
     'ecosafety.grammar.core.2026v1', 'SQLSCHEMA',
     'eco_restoration_shard',
     'db/dbecosafetygrammarcore.sql', 'dbecosafetygrammarcore.sql',
     'SQLite',
     '2026v1', 1, 'all', 'CONSTELLATION',
     'Canonical ecosafety grammar (planes, risk coordinates, corridors, residualkernel, residualterm).',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     '2026-05-03T07:15:00Z', '2026-05-03T07:15:00Z'),

    -- Plane weights shard (ALN)
    ('EcosafetyPlaneWeights2026v1', 'PLANEWEIGHTS',
     'ecosafety.planeweights.2026v1', 'ALNSCHEMA',
     'eco_restoration_shard',
     'aln/ecosafetyPlaneWeightsShard2026v1.aln', 'ecosafetyPlaneWeightsShard2026v1.aln',
     'ALN',
     '2026v1', 1, 'all', 'CONSTELLATION',
     'Plane weights and non-compensation contract including topology plane.',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     '2026-05-03T07:15:00Z', '2026-05-03T07:15:00Z'),

    -- Artifact registry schema
    ('ArtifactRegistry2026v1', 'ARTIFACTREGISTRY',
     'econet.artifact.registry.sql.2026v1', 'SQLSCHEMA',
     'eco_restoration_shard',
     'db/dbartifactregistry.sql', 'dbartifactregistry.sql',
     'SQLite',
     '2026v1', 1, 'dataquality', 'REPO',
     'Universal artifact registry and provenance tables for all governed artifacts.',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     '2026-05-03T07:15:00Z', '2026-05-03T07:15:00Z'),

    -- Artifact registry helper views
    ('ArtifactRegistry2026v1', 'ARTIFACTREGISTRY',
     'econet.artifact.registry.index.sql.2026v1', 'SQLVIEW',
     'eco_restoration_shard',
     'db/dbartifactregistryindex.sql', 'dbartifactregistryindex.sql',
     'SQLite',
     '2026v1', 1, 'dataquality', 'CONSTELLATION',
     'Helper views joining artifactregistry to shardinstance, planeweights, blastradius, and provenance.',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     '2026-05-03T07:15:00Z', '2026-05-03T07:15:00Z');

-------------------------------------------------------------------------------
-- 4. StewardEcoWealthStatement schema and registry entry
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS stewardecowealthstatement (
    statementid         INTEGER PRIMARY KEY AUTOINCREMENT,
    stewarddid          TEXT NOT NULL,
    regioncode          TEXT NOT NULL,
    kercontext          TEXT NOT NULL,
    epochstartutc       TEXT NOT NULL,
    epochendutc         TEXT NOT NULL,
    primaryrepoid       INTEGER,
    primaryshardid      INTEGER,
    evidencehex         TEXT NOT NULL,
    signingdid          TEXT NOT NULL,
    kmetric             REAL NOT NULL,
    emetric             REAL NOT NULL,
    rmetric             REAL NOT NULL,
    vtmax               REAL NOT NULL,
    sregion             REAL NOT NULL,
    wealthtier          TEXT NOT NULL CHECK (wealthtier IN ('SAFE','GOLD','HARD','BLOCKED')),
    ecounitsgross       REAL NOT NULL,
    ecounitsnet         REAL NOT NULL,
    carbonbenefitt      REAL NOT NULL,
    waterbenefitm3      REAL NOT NULL,
    materialsoffsetkg   REAL NOT NULL,
    biodiversityscore   REAL NOT NULL,
    dataqualityfactor   REAL NOT NULL,
    uncertaintyfactor   REAL NOT NULL,
    governancepenalty   REAL NOT NULL,
    kerdeployable       INTEGER NOT NULL CHECK (kerdeployable IN (0,1)),
    corridorbreaches    INTEGER NOT NULL,
    notes               TEXT,
    CREATEUTC           TEXT NOT NULL DEFAULT (datetime('now')),
    UPDATEUTC           TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_stewardwealth_steward_region_epoch
    ON stewardecowealthstatement (stewarddid, regioncode, epochstartutc, epochendutc);

CREATE INDEX IF NOT EXISTS idx_stewardwealth_ker
    ON stewardecowealthstatement (kmetric, emetric, rmetric);

CREATE INDEX IF NOT EXISTS idx_stewardwealth_region_sregion
    ON stewardecowealthstatement (regioncode, sregion);

CREATE INDEX IF NOT EXISTS idx_stewardwealth_evidence
    ON stewardecowealthstatement (evidencehex);

INSERT OR IGNORE INTO definitionregistry (
    contractid, scope, logicalname, kind,
    repo, destinationpath, filename, language,
    versiontag, active, primaryplane, appliescope,
    summary, signingdid, issuedutc, updatedutc
) VALUES
    ('EcoWealthStewardStatement2026v1', 'ECOWEALTH',
     'ecowealth.steward.statement.sql.2026v1', 'SQLSCHEMA',
     'eco_restoration_shard',
     'db/dbstewardecowealthstatement.sql', 'dbstewardecowealthstatement.sql',
     'SQLite',
     '2026v1', 1, 'all', 'STEWARD',
     'Steward eco-wealth statements per region and KER context (EcoUnits, K/E/R, evidence hex).',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     '2026-05-17T07:15:00Z', '2026-05-17T07:15:00Z');

-------------------------------------------------------------------------------
-- 5. vshardresidual view binding (Band-1 math spine)
-------------------------------------------------------------------------------

-- Assumptions:
-- - planeweightsplane(contractid, planeid, weight, nonoffsettable, ...)
-- - residualterm(kernelid, coordid, alpha, ...)
-- - shardriskvalue(shardid, coordid, rvalue)
-- - shardinstance(shardid, planecontractid, ...)
-- The exact core tables live in other migrations; this view only references them.

CREATE VIEW IF NOT EXISTS vshardriskvector AS
SELECT
    rv.shardid     AS shardid,
    rv.coordid     AS coordid,
    rc.planeid     AS planeid,
    CAST(ROUND(rv.rvalue, 6) AS REAL) AS rvalue
FROM shardriskvalue AS rv
JOIN riskcoordinate AS rc
    ON rc.coordid = rv.coordid;

CREATE VIEW IF NOT EXISTS vshardresidual AS
SELECT
    s.shardid                 AS shardid,
    s.planecontractid         AS planecontractid,
    SUM(pw.weight * rt.alpha * rv.rvalue * rv.rvalue) AS vt,
    MAX(rv.rvalue)            AS rmax,
    1.0 - MAX(rv.rvalue)      AS evalue
FROM shardinstance AS s
JOIN vshardriskvector AS rv
    ON rv.shardid = s.shardid
JOIN residualterm AS rt
    ON rt.coordid = rv.coordid
JOIN planeweightsplane AS pw
    ON pw.planeid = rv.planeid
   AND pw.contractid = s.planecontractid
GROUP BY
    s.shardid,
    s.planecontractid;

INSERT OR IGNORE INTO definitionregistry (
    contractid, scope, logicalname, kind,
    repo, destinationpath, filename, language,
    versiontag, active, primaryplane, appliescope,
    summary, signingdid, issuedutc, updatedutc
) VALUES
    ('EcoWealthMathSpine2026v1', 'ECOWEALTHMATH',
     'ecosafety.vshardresidual.view.2026v1', 'SQLVIEW',
     'eco_restoration_shard',
     'db/dbecosafetymathspineband1.sql', 'dbecosafetymathspineband1.sql',
     'SQLite',
     '2026v1', 1, 'all', 'CONSTELLATION',
     'Canonical shard residual view vshardresidual (Vt, R, E) for Band-1 math spine.',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     '2026-05-17T07:15:00Z', '2026-05-17T07:15:00Z');

-------------------------------------------------------------------------------
-- 6. Eco-wealth view over steward statements and residuals
-------------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vecowealthview AS
SELECT
    s.stewarddid,
    s.regioncode,
    s.kercontext,
    s.epochstartutc,
    s.epochendutc,
    s.kmetric,
    s.emetric,
    s.rmetric,
    s.vtmax,
    s.sregion,
    s.wealthtier,
    s.ecounitsgross,
    s.ecounitsnet,
    s.carbonbenefitt,
    s.waterbenefitm3,
    s.materialsoffsetkg,
    s.biodiversityscore,
    s.dataqualityfactor,
    s.uncertaintyfactor,
    s.governancepenalty,
    s.kerdeployable,
    s.corridorbreaches,
    s.evidencehex,
    s.signingdid
FROM stewardecowealthstatement AS s;

INSERT OR IGNORE INTO definitionregistry (
    contractid, scope, logicalname, kind,
    repo, destinationpath, filename, language,
    versiontag, active, primaryplane, appliescope,
    summary, signingdid, issuedutc, updatedutc
) VALUES
    ('EcoWealthMathSpine2026v1', 'ECOWEALTHMATH',
     'ecowealth.view.steward.2026v1', 'SQLVIEW',
     'eco_restoration_shard',
     'db/dbdefinitionregistry.sql', 'dbdefinitionregistry.sql',
     'SQLite',
     '2026v1', 1, 'all', 'STEWARD',
     'Eco-wealth steward view vecowealthview over stewardecowealthstatement (KER and EcoUnits).',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     '2026-05-17T07:15:00Z', '2026-05-17T07:15:00Z');
