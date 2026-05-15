-- filename: db_restoration_contracts_planes.sql
-- destination: eco_restoration_shard/db/db_restoration_contracts_planes.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS restorationcontract (
    contractid        INTEGER PRIMARY KEY AUTOINCREMENT,
    contractname      TEXT NOT NULL,
    versiontag        TEXT NOT NULL,
    status            TEXT NOT NULL,  -- ACTIVE, DEPRECATED, EXPERIMENTAL
    bostrom_address   TEXT NOT NULL,
    region            TEXT NOT NULL,
    scope             TEXT NOT NULL,  -- CONSTELLATION, REGION, NODE
    kerdeployable     INTEGER NOT NULL DEFAULT 0 CHECK (kerdeployable IN (0,1)),
    prodeligible      INTEGER NOT NULL DEFAULT 0 CHECK (prodeligible IN (0,1)),
    createdutc        TEXT NOT NULL,
    updatedutc        TEXT NOT NULL,
    UNIQUE (contractname, versiontag, region)
);

CREATE TABLE IF NOT EXISTS restorationplane (
    planeid           INTEGER PRIMARY KEY AUTOINCREMENT,
    plane_name        TEXT NOT NULL,
    region            TEXT NOT NULL,
    scope             TEXT NOT NULL,  -- CONSTELLATION, REGION, NODE
    lane              TEXT NOT NULL,  -- BUILD, RESEARCH, EXPPROD, PROD
    kmetric           REAL NOT NULL,
    emetric           REAL NOT NULL,
    rmetric           REAL NOT NULL,
    vtresidual        REAL NOT NULL,
    kerdeployable     INTEGER NOT NULL DEFAULT 0 CHECK (kerdeployable IN (0,1)),
    prodeligible      INTEGER NOT NULL DEFAULT 0 CHECK (prodeligible IN (0,1)),
    createdutc        TEXT NOT NULL,
    updatedutc        TEXT NOT NULL
);

CREATE VIEW IF NOT EXISTS v_active_restoration_contracts_primary AS
SELECT
    contractid,
    contractname,
    versiontag,
    status,
    bostrom_address,
    region,
    scope,
    kerdeployable,
    prodeligible,
    createdutc,
    updatedutc
FROM restorationcontract
WHERE status = 'ACTIVE'
  AND bostrom_address = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7';

CREATE VIEW IF NOT EXISTS v_prod_eligible_restoration_planes AS
SELECT
    planeid,
    plane_name,
    region,
    scope,
    lane,
    kmetric,
    emetric,
    rmetric,
    vtresidual,
    kerdeployable,
    prodeligible,
    createdutc,
    updatedutc
FROM restorationplane
WHERE prodeligible = 1
  AND kerdeployable = 1
  AND lane = 'PROD'
  AND kmetric >= 0.90
  AND emetric >= 0.90
  AND rmetric <= 0.13;
