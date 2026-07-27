-- filename: sql/blastradius_outbox_schema.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS blastradius_outbox (
    outbox_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    diagid        TEXT NOT NULL UNIQUE,
    created_utc   TEXT NOT NULL,
    processed_utc TEXT,
    status        TEXT NOT NULL CHECK (status IN ('PENDING','PROCESSED','FAILED')),
    last_error    TEXT,
    FOREIGN KEY (diagid) REFERENCES blastradiusdiag(diagid)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_blastradius_outbox_status_id
    ON blastradius_outbox (status, outbox_id);
