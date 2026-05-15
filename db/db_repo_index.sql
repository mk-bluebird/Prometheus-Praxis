-- filename: db_repo_index.sql
-- destination: eco_restoration_shard/db/db_repo_index.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS repo (
    repoid      INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL UNIQUE,
    roleband    TEXT NOT NULL,  -- e.g. 'RESTORATION_MONO', 'SPINE_CLIENT'
    description TEXT NOT NULL,
    region      TEXT NOT NULL,  -- e.g. 'Phoenix-AZ', 'Global'
    createdutc  TEXT NOT NULL,
    updatedutc  TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS repofile (
    fileid      INTEGER PRIMARY KEY AUTOINCREMENT,
    repoid      INTEGER NOT NULL,
    relpath     TEXT NOT NULL,
    purpose     TEXT NOT NULL,
    language    TEXT NOT NULL,
    createdutc  TEXT NOT NULL,
    updatedutc  TEXT NOT NULL,
    UNIQUE (repoid, relpath),
    FOREIGN KEY (repoid) REFERENCES repo(repoid) ON DELETE CASCADE
);

INSERT OR IGNORE INTO repo (
    name,
    roleband,
    description,
    region,
    createdutc,
    updatedutc
) VALUES (
    'eco_restoration_shard',
    'RESTORATION_MONO',
    'Mono-repo for EcoNet eco-restoration projects and shards, mirroring Eco-Fort governance patterns.',
    'Phoenix-AZ',
    datetime('now'),
    datetime('now')
);
