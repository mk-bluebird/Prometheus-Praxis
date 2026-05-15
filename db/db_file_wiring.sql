-- filename: db_file_wiring.sql
-- destination: eco_restoration_shard/db/db_file_wiring.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS filewiring (
    wiringid       INTEGER PRIMARY KEY AUTOINCREMENT,
    repofileid     INTEGER NOT NULL,
    logicalrole    TEXT NOT NULL,   -- e.g. 'SQL_SCHEMA', 'ALN_PARTICLE', 'DOC_SPEC', 'CPP_CORE'
    modulegroup    TEXT NOT NULL,   -- e.g. 'RESTORATION_PLANE', 'LANE_STATUS', 'BLAST_RADIUS'
    depends_on     TEXT,            -- comma-separated logicalnames or DR codes
    lane_band      TEXT NOT NULL,   -- RESEARCH, EXPPROD, PROD, GOV
    nonactuating   INTEGER NOT NULL DEFAULT 1 CHECK (nonactuating IN (0,1)),
    createdutc     TEXT NOT NULL,
    updatedutc     TEXT NOT NULL,
    FOREIGN KEY (repofileid) REFERENCES repofile(fileid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_filewiring_role
    ON filewiring (logicalrole, modulegroup);
