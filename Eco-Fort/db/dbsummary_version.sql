-- filename dbsummary_version.sql
-- destination Eco-Fort/db/dbsummary_version.sql
-- repo-target github.com/mk-bluebird/Eco-Fort

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS shard_summary_meta (
  shard_id            TEXT PRIMARY KEY,
  summary_version     TEXT NOT NULL,        -- UUID string
  summary_updated_utc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS shard_summary (
  shard_id        TEXT PRIMARY KEY,
  summary_version TEXT NOT NULL,            -- FK into shard_summary_meta.summary_version
  summary_json    TEXT NOT NULL,
  created_utc     TEXT NOT NULL,
  updated_utc     TEXT NOT NULL,
  FOREIGN KEY (shard_id) REFERENCES shard_summary_meta(shard_id)
    ON DELETE CASCADE
);
