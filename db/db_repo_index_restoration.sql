-- filename: db_repo_index_restoration.sql
-- destination: eco_restoration_shard/db/db_repo_index_restoration.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS repo (
    repoid      INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL UNIQUE,
    roleband    TEXT NOT NULL,
    description TEXT NOT NULL,
    region      TEXT NOT NULL,
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
    'EcoNet eco-restoration mono-repo for Phoenix-AZ, providing restorationindex governance DB and non-actuating query tools.',
    'Phoenix-AZ',
    datetime('now'),
    datetime('now')
);

INSERT OR IGNORE INTO repofile (
    repoid,
    relpath,
    purpose,
    language,
    createdutc,
    updatedutc
)
SELECT
    r.repoid,
    'db/restorationindex.sqlite3',
    'GOVERNANCE_DB',
    'sqlite3',
    datetime('now'),
    datetime('now')
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';
