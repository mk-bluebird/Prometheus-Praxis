-- filename: db_definition_registry_restoration.sql
-- destination: eco_restoration_shard/db/db_definition_registry_restoration.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS definitionregistry_restoration (
    definitionid     INTEGER PRIMARY KEY AUTOINCREMENT,
    logicalname      TEXT NOT NULL,
    versiontag       TEXT NOT NULL,
    hash             TEXT NOT NULL,
    status           TEXT NOT NULL, -- FROZENACTIVE, FROZENDEPRECATED, EXPERIMENTAL
    repoid           INTEGER NOT NULL,
    relpath_sql      TEXT,
    relpath_aln      TEXT,
    relpath_doc      TEXT,
    ecoscope         TEXT NOT NULL, -- e.g. 'RESTORATION_CORE', 'PLANE', 'LANE', 'BLAST'
    contractid       TEXT NOT NULL, -- e.g. 'EcosafetyContinuity2026v1'
    createdutc       TEXT NOT NULL,
    updatedutc       TEXT NOT NULL,
    UNIQUE (logicalname, versiontag),
    FOREIGN KEY (repoid) REFERENCES repo(repoid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_definitionregistry_restoration_logical
    ON definitionregistry_restoration (logicalname, status);
