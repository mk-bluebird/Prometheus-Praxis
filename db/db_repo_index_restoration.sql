-- filename: db/dbrepoindexrestoration.sql
-- destination: eco_restoration_shard/db/dbrepoindexrestoration.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS repo (
    repoid       INTEGER PRIMARY KEY AUTOINCREMENT,
    name         TEXT NOT NULL UNIQUE,
    roleband     TEXT NOT NULL,
    description  TEXT NOT NULL,
    region       TEXT NOT NULL,
    created_utc  TEXT NOT NULL,
    updated_utc  TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS repofile (
    fileid       INTEGER PRIMARY KEY AUTOINCREMENT,
    repoid       INTEGER NOT NULL,
    relpath      TEXT NOT NULL,
    purpose      TEXT NOT NULL,
    language     TEXT NOT NULL,
    created_utc  TEXT NOT NULL,
    updated_utc  TEXT NOT NULL,
    UNIQUE (repoid, relpath),
    FOREIGN KEY (repoid) REFERENCES repo(repoid) ON DELETE CASCADE
);

INSERT OR IGNORE INTO repo (
    name,
    roleband,
    description,
    region,
    created_utc,
    updated_utc
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
    created_utc,
    updated_utc
)
SELECT
    r.repoid,
    'db/dbrepoindexrestoration.sql',
    'SCHEMA_SQL',
    'sqlite3',
    datetime('now'),
    datetime('now')
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

INSERT OR IGNORE INTO repofile (
    repoid,
    relpath,
    purpose,
    language,
    created_utc,
    updated_utc
)
SELECT
    r.repoid,
    'db/dbbostromprovenance.sql',
    'SCHEMA_SQL',
    'sqlite3',
    datetime('now'),
    datetime('now')
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

INSERT OR IGNORE INTO repofile (
    repoid,
    relpath,
    purpose,
    language,
    created_utc,
    updated_utc
)
SELECT
    r.repoid,
    'db/dbrestorationidentitybinding.sql',
    'SCHEMA_SQL',
    'sqlite3',
    datetime('now'),
    datetime('now')
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

INSERT OR IGNORE INTO repofile (
    repoid,
    relpath,
    purpose,
    language,
    created_utc,
    updated_utc
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

INSERT OR IGNORE INTO repofile (
    repoid,
    relpath,
    purpose,
    language,
    created_utc,
    updated_utc
)
SELECT
    r.repoid,
    'src/bin/restorationindextool.rs',
    'TOOL_BIN',
    'Rust',
    datetime('now'),
    datetime('now')
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';
