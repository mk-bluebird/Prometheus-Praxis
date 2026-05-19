-- filename: db/objext/objext_schema.sql
-- destination: ecorestorationshard/db/objext/objext_schema.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS objext_session (
    session_id     TEXT PRIMARY KEY,
    user_id        TEXT,
    source         TEXT NOT NULL,
    created_at     TEXT NOT NULL,   -- RFC3339
    input_summary  TEXT NOT NULL,
    profile        TEXT NOT NULL,
    shard_id       TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS objext_object (
    session_id        TEXT NOT NULL,
    section_index     INTEGER NOT NULL,   -- 1..10
    obj_index         INTEGER NOT NULL,   -- 1..100
    obj_type          TEXT NOT NULL,      -- topic|question|quiz|definition|task
    title             TEXT NOT NULL,
    prompt            TEXT NOT NULL,
    complexity_reason TEXT NOT NULL,
    priority          INTEGER NOT NULL,   -- 1..5
    tags              TEXT NOT NULL,      -- JSON array of strings
    PRIMARY KEY (session_id, obj_index),
    FOREIGN KEY (session_id) REFERENCES objext_session(session_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_objext_object_session_section
    ON objext_object (session_id, section_index);
