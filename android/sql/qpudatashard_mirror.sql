-- Filename: android/sql/qpudatashard_mirror.sql

CREATE TABLE IF NOT EXISTS qpudatashard (
    id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    node_id            TEXT    NOT NULL,
    window_start_ts    TEXT    NOT NULL,
    window_end_ts      TEXT    NOT NULL,
    ker_k              REAL    NOT NULL,
    ker_e              REAL    NOT NULL,
    ker_r              REAL    NOT NULL,
    vt                 REAL    NOT NULL,
    corridor_status    TEXT    NOT NULL, -- JSON of per-coordinate statuses
    evidencehex        TEXT    NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_qpudatashard_node_window
    ON qpudatashard (node_id, window_end_ts);

CREATE TABLE IF NOT EXISTS maintenance_event (
    id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    node_id            TEXT    NOT NULL,
    event_ts           TEXT    NOT NULL,
    engineer_id        TEXT    NOT NULL,
    event_type         TEXT    NOT NULL,  -- e.g. "inspection", "cleaning"
    notes              TEXT,
    photo_uri          TEXT,
    evidencehex        TEXT    NOT NULL,
    synced_to_core     INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_maintenance_node_ts
    ON maintenance_event (node_id, event_ts);
