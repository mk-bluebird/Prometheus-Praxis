-- filename: eco_restoration_shard/crates/cyboquatic_progress/20260707_crate/sql/daily_progress_init.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS daily_progress (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    date_str        TEXT NOT NULL,
    domain          TEXT NOT NULL,
    sub_task        TEXT NOT NULL,
    evidence_hex    TEXT NOT NULL,
    k               REAL NOT NULL,
    e               REAL NOT NULL,
    r               REAL NOT NULL,
    prior_pointer   TEXT NOT NULL,
    indicator_json  TEXT NOT NULL,
    created_at      TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_daily_progress_date
    ON daily_progress (date_str);

CREATE INDEX IF NOT EXISTS idx_daily_progress_domain
    ON daily_progress (domain, sub_task);
