-- FILE: ecorestorationshard/sqlspine/dbmigrationaudit.csv.sql
-- DESTINATION: ecorestorationshard/sqlspine/dbmigrationaudit.csv.sql
-- REPO-TARGET: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS migrationaudit (
    auditid INTEGER PRIMARY KEY AUTOINCREMENT,
    filerelpath TEXT NOT NULL,
    repotarget TEXT NOT NULL,
    previousowner TEXT NOT NULL,
    newowner TEXT NOT NULL,
    migratedutc TEXT NOT NULL,
    notes TEXT
);

CREATE INDEX IF NOT EXISTS idx_migrationaudit_file
    ON migrationaudit(filerelpath, repotarget);
