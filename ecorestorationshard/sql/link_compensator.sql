-- link_compensator.sql
-- Read-only resolution log and cache for broken or live URLs.
-- Monotone: new rows only add evidence, never mutate prior records.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS link_resolution_log (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    url                 TEXT NOT NULL,
    fetched_status      TEXT NOT NULL, -- e.g. "200", "404", "timeout", "archive", "heuristic"
    source_kind         TEXT NOT NULL, -- "CITY_GIS", "COUNTY_GIS", "STATE_WILDLIFE", "FED_CRIT_HABITAT", "FED_WETLANDS", "UNKNOWN"
    archive_url         TEXT,          -- if resolved via Wayback or alt portal
    snapshot_ts_utc     TEXT,          -- ISO8601 for when snapshot / resolution was observed
    eco_impact_score    REAL NOT NULL, -- 0.0 - 1.0, corridor-governed
    energy_saved_kwh    REAL,          -- nullable, monotone estimates
    co2_offset_kg       REAL,
    material_recyclability TEXT,       -- e.g. "NEUTRAL", "POSITIVE", "N/A"
    hex_proof           TEXT NOT NULL, -- sha256 over URL + fetched_status + source_kind + eco_impact_score
    created_at_utc      TEXT NOT NULL  -- insertion time, never updated
);

CREATE INDEX IF NOT EXISTS idx_link_resolution_log_url
    ON link_resolution_log (url);

CREATE INDEX IF NOT EXISTS idx_link_resolution_log_source_kind
    ON link_resolution_log (source_kind);


CREATE TABLE IF NOT EXISTS compensated_data_cache (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    resolution_id       INTEGER NOT NULL,
    key                 TEXT NOT NULL,   -- e.g. "DATA_TYPE", "FORMAT", "ACCESS_PATTERN", "GOVERNANCE_HINT"
    value               TEXT NOT NULL,   -- JSON or plain text description
    created_at_utc      TEXT NOT NULL,
    FOREIGN KEY (resolution_id) REFERENCES link_resolution_log(id)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_compensated_data_cache_resolution
    ON compensated_data_cache (resolution_id);

CREATE INDEX IF NOT EXISTS idx_compensated_data_cache_key
    ON compensated_data_cache (key);
