-- filename: db_restorationdbshard.sql
-- destination: eco_restoration_shard/db/db_restorationdbshard.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS restorationdbshard (
    dbshardid      INTEGER PRIMARY KEY AUTOINCREMENT,
    logicalname    TEXT NOT NULL,
    description    TEXT NOT NULL,
    region         TEXT NOT NULL,
    scope          TEXT NOT NULL,  -- CONSTELLATION, REGION, NODE
    dbrole         TEXT NOT NULL,  -- GOVERNANCE, TELEMETRY, INDEX
    connectionstr  TEXT NOT NULL,  -- e.g. file:db/restorationindex.sqlite3?mode=ro
    contractid     TEXT NOT NULL,  -- e.g. EcosafetyContinuity2026v1
    definitionlogical TEXT NOT NULL,
    readonly       INTEGER NOT NULL DEFAULT 1 CHECK (readonly IN (0,1)),
    active         INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    createdutc     TEXT NOT NULL,
    updatedutc     TEXT NOT NULL,
    UNIQUE (logicalname, region, scope, dbrole)
);

INSERT OR IGNORE INTO restorationdbshard (
    logicalname,
    description,
    region,
    scope,
    dbrole,
    connectionstr,
    contractid,
    definitionlogical,
    readonly,
    active,
    createdutc,
    updatedutc
) VALUES (
    'restorationindex',
    'EcoNet eco-restoration governance DB for Phoenix-AZ region (restorationindex.sqlite3).',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCE',
    'file:db/restorationindex.sqlite3?mode=ro',
    'EcosafetyContinuity2026v1',
    'restoration.governance.index.2026v1',
    1,
    1,
    datetime('now'),
    datetime('now')
);
