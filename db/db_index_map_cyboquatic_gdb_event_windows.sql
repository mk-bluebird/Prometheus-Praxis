-- filename: db_index_map_cyboquatic_gdb_event_windows.sql
-- destination: eco_restoration_shard/db/db_index_map_cyboquatic_gdb_event_windows.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS econet_sql_index (
    sql_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    repotarget      TEXT NOT NULL,     -- e.g. "github.com/mk-bluebird/eco_restoration_shard"
    destinationpath TEXT NOT NULL,     -- directory within repo, e.g. "db/"
    filename        TEXT NOT NULL,     -- e.g. "db_cyboquatic_gdb_event_windows.sql"
    description     TEXT NOT NULL,
    active          INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    created_utc     TEXT NOT NULL,
    updated_utc     TEXT NOT NULL,
    UNIQUE (repotarget, destinationpath, filename)
);

INSERT OR IGNORE INTO econet_sql_index (
    repotarget,
    destinationpath,
    filename,
    description,
    created_utc,
    updated_utc
) VALUES (
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/',
    'db_cyboquatic_gdb_event_windows.sql',
    'Cyboquatic GDB + restoration event-window schema: GDB instances, blast-radius links, window metrics, migration plans, neurorights envelopes, and eco-placement objective ledger.',
    datetime('now'),
    datetime('now')
);
