-- filename: db_restoration_shard_catalog.sql
-- destination: eco_restoration_shard/db/db_restoration_shard_catalog.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS restorationdbshard (
    dbshardid        INTEGER PRIMARY KEY AUTOINCREMENT,
    logicalname      TEXT NOT NULL,  -- e.g. 'restorationindex', 'lanestatus_restoration'
    description      TEXT NOT NULL,
    repoid           INTEGER NOT NULL,
    repofileid       INTEGER NOT NULL,
    region           TEXT NOT NULL,
    scope            TEXT NOT NULL,  -- CONSTELLATION, REGION, NODE
    dbrole           TEXT NOT NULL,  -- TELEMETRY, GOVERNANCE, INDEX
    connectionstring TEXT NOT NULL,  -- e.g. 'filerestorationindex.sqlite3?mode=ro'
    readonly         INTEGER NOT NULL DEFAULT 1 CHECK (readonly IN (0,1)),
    active           INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    createdutc       TEXT NOT NULL,
    updatedutc       TEXT NOT NULL,
    UNIQUE (logicalname, region),
    FOREIGN KEY (repoid) REFERENCES repo(repoid) ON DELETE CASCADE,
    FOREIGN KEY (repofileid) REFERENCES repofile(fileid) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS restorationdb_definition_binding (
    bindingid    INTEGER PRIMARY KEY AUTOINCREMENT,
    dbshardid    INTEGER NOT NULL,
    definitionid INTEGER NOT NULL,
    bindingrole  TEXT NOT NULL,   -- PRIMARYIMPL, MIRROR, SHADOW
    createdutc   TEXT NOT NULL,
    UNIQUE (dbshardid, definitionid),
    FOREIGN KEY (dbshardid) REFERENCES restorationdbshard(dbshardid) ON DELETE CASCADE,
    FOREIGN KEY (definitionid) REFERENCES definitionregistry_restoration(definitionid)
        ON DELETE CASCADE
);
