-- filename: dbmigration_audit.csv.sql
-- destination: eco_restoration_shard/sql/spine/dbmigration_audit.csv.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS migration_audit (
    audit_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    file_relpath    TEXT NOT NULL,
    repo_target     TEXT NOT NULL,
    previous_owner  TEXT NOT NULL,
    new_owner       TEXT NOT NULL,
    migrated_utc    TEXT NOT NULL,
    notes           TEXT
);

CREATE INDEX IF NOT EXISTS idx_migration_audit_file
    ON migration_audit(file_relpath, repo_target);
