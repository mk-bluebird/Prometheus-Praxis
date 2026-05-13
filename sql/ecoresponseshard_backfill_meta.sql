-- filename: sql/ecoresponseshard_backfill_meta.sql
-- destination: ecoresponseshard/sql/ecoresponseshard_backfill_meta.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS responsebackfillmeta (
  metaid              INTEGER PRIMARY KEY AUTOINCREMENT,

  -- Source econet-index DB identity
  econetdbpath        TEXT    NOT NULL,  -- filesystem path or logical name
  econetdb_hash       TEXT    NOT NULL,  -- hex digest of the econet-index DB file

  -- Temporal snapshot
  snapshotutc         TEXT    NOT NULL,  -- ISO8601 when snapshot was taken

  -- Backfill tool identity
  backfill_version    TEXT    NOT NULL,  -- e.g., ecoresponseshard-backfill 2026.05.12
  backfill_spechash   TEXT    NOT NULL,  -- ALNSPECHASHHEX of the backfill tool spec

  -- Shard id coverage
  shardid_min         INTEGER NOT NULL,
  shardid_max         INTEGER NOT NULL,
  shard_count         INTEGER NOT NULL,

  -- Optional topic/lane scope for partial runs
  lane_scope          TEXT,             -- e.g., RESEARCH, EXPPROD, PROD, or ALL
  region_scope        TEXT,             -- e.g., Phoenix-AZ, or ALL

  -- Provenance and commentary
  evidencehex         TEXT,              -- hash of backfill transcript/log
  comment             TEXT
);

CREATE INDEX IF NOT EXISTS idx_responsebackfillmeta_snapshot
  ON responsebackfillmeta (snapshotutc);

CREATE INDEX IF NOT EXISTS idx_responsebackfillmeta_dbhash
  ON responsebackfillmeta (econetdb_hash);
