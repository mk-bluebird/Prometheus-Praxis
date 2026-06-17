-- Filename: sql/db_governance_core_2026v1.sql
-- Destination: sql/db_governance_core_2026v1.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS definitionregistry (
    defid TEXT PRIMARY KEY,
    logicalname TEXT NOT NULL,
    repopath TEXT NOT NULL,
    category TEXT NOT NULL,
    frozen INTEGER NOT NULL DEFAULT 0 CHECK (frozen IN (0, 1))
);

CREATE TABLE IF NOT EXISTS grammarartifact (
    artifactid INTEGER PRIMARY KEY AUTOINCREMENT,
    defid TEXT NOT NULL REFERENCES definitionregistry(defid) ON DELETE CASCADE,
    kind TEXT NOT NULL,
    repopath TEXT NOT NULL,
    destination TEXT NOT NULL,
    filename TEXT NOT NULL,
    language TEXT NOT NULL,
    versiontag TEXT NOT NULL,
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    summary TEXT NOT NULL,
    UNIQUE (defid, kind, destination, filename, versiontag)
);

CREATE INDEX IF NOT EXISTS idx_grammarartifact_defid
    ON grammarartifact (defid, active);

CREATE TABLE IF NOT EXISTS lanestatusshard (
    shard_id TEXT NOT NULL,
    region TEXT NOT NULL,
    lane TEXT NOT NULL,
    verdict TEXT NOT NULL,
    ker_k REAL NOT NULL,
    ker_e REAL NOT NULL,
    ker_r REAL NOT NULL,
    residual_vt REAL NOT NULL,
    max_staleness_hours INTEGER NOT NULL,
    expires_utc INTEGER NOT NULL,
    carbonnegativeok INTEGER NOT NULL CHECK (carbonnegativeok IN (0, 1)),
    restorationok INTEGER NOT NULL CHECK (restorationok IN (0, 1)),
    applied_utc INTEGER NOT NULL,
    PRIMARY KEY (shard_id, region, applied_utc)
);

CREATE VIEW IF NOT EXISTS vlanestatuslatest AS
SELECT ls.*
FROM lanestatusshard ls
JOIN (
    SELECT shard_id,
           region,
           MAX(applied_utc) AS max_applied_utc
    FROM lanestatusshard
    GROUP BY shard_id, region
) latest
ON ls.shard_id = latest.shard_id
AND ls.region = latest.region
AND ls.applied_utc = latest.max_applied_utc;

CREATE VIEW IF NOT EXISTS vlanepromotionhistory AS
SELECT shard_id,
       region,
       lane,
       verdict,
       ker_k,
       ker_e,
       ker_r,
       residual_vt,
       applied_utc
FROM lanestatusshard;

CREATE TABLE IF NOT EXISTS kerresidual (
    shard_id TEXT NOT NULL,
    region TEXT NOT NULL,
    ker_k REAL NOT NULL,
    ker_e REAL NOT NULL,
    ker_r REAL NOT NULL,
    residual_vt REAL NOT NULL,
    PRIMARY KEY (shard_id, region)
);

CREATE VIEW IF NOT EXISTS vresidualkernel AS
SELECT shard_id,
       region,
       ker_k,
       ker_e,
       ker_r,
       residual_vt
FROM kerresidual;

CREATE TABLE IF NOT EXISTS planeweights (
    shard_id TEXT NOT NULL,
    plane_name TEXT NOT NULL,
    weight REAL NOT NULL,
    non_offsettable INTEGER NOT NULL CHECK (non_offsettable IN (0, 1)),
    PRIMARY KEY (shard_id, plane_name)
);

CREATE VIEW IF NOT EXISTS vplaneweights AS
SELECT shard_id,
       plane_name,
       weight,
       non_offsettable
FROM planeweights;

CREATE TABLE IF NOT EXISTS blastradiusindex (
    shard_id TEXT NOT NULL PRIMARY KEY,
    radius_meters REAL NOT NULL,
    adjacency_count INTEGER NOT NULL
);

CREATE VIEW IF NOT EXISTS vshardblast AS
SELECT shard_id,
       radius_meters,
       adjacency_count
FROM blastradiusindex;

CREATE TABLE IF NOT EXISTS ecounitshard2026v1 (
    unit_id TEXT NOT NULL PRIMARY KEY,
    steward_did TEXT NOT NULL,
    shard_id TEXT NOT NULL,
    eco_unit_score REAL NOT NULL,
    k_effective REAL NOT NULL,
    e_effective REAL NOT NULL,
    r_effective REAL NOT NULL
);

CREATE TABLE IF NOT EXISTS stewardecowealthstatement (
    steward_did TEXT NOT NULL,
    shard_id TEXT NOT NULL,
    wealth_score REAL NOT NULL,
    k_effective REAL NOT NULL,
    e_effective REAL NOT NULL,
    r_effective REAL NOT NULL,
    recorded_utc INTEGER NOT NULL,
    PRIMARY KEY (steward_did, shard_id, recorded_utc)
);

CREATE VIEW IF NOT EXISTS vsteward_eco_wealth_latest AS
SELECT sew.steward_did,
       sew.shard_id,
       sew.wealth_score,
       sew.k_effective,
       sew.e_effective,
       sew.r_effective
FROM stewardecowealthstatement sew
JOIN (
    SELECT steward_did,
           shard_id,
           MAX(recorded_utc) AS max_recorded_utc
    FROM stewardecowealthstatement
    GROUP BY steward_did, shard_id
) latest
ON sew.steward_did = latest.steward_did
AND sew.shard_id = latest.shard_id
AND sew.recorded_utc = latest.max_recorded_utc;

CREATE TABLE IF NOT EXISTS cyboquaticecokernel (
    node_id TEXT NOT NULL PRIMARY KEY,
    shard_id TEXT NOT NULL,
    eco_per_joule REAL NOT NULL,
    carbonnegativeok INTEGER NOT NULL CHECK (carbonnegativeok IN (0, 1)),
    restorationok INTEGER NOT NULL CHECK (restorationok IN (0, 1))
);

CREATE VIEW IF NOT EXISTS vcyboquaticecoperjoule AS
SELECT node_id,
       shard_id,
       eco_per_joule,
       carbonnegativeok,
       restorationok
FROM cyboquaticecokernel;

INSERT INTO definitionregistry (defid, logicalname, repopath, category, frozen)
VALUES
    ('econet.db.governance.core.2026v1',
     'EcoNet Governance Core DB 2026v1',
     'sql/db_governance_core_2026v1.sql',
     'SQL',
     1)
ON CONFLICT(defid) DO NOTHING;
