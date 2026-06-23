-- filename: db_ecocybo_fileindex.sql
-- destination: EcoNet/db/db_ecocybo_fileindex.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS econetfileindex (
    fileindexid  INTEGER PRIMARY KEY AUTOINCREMENT,
    filename     TEXT NOT NULL,
    destination  TEXT NOT NULL,
    repotarget   TEXT NOT NULL,
    roleband     TEXT NOT NULL,
    lanedefault  TEXT NOT NULL,
    description  TEXT NOT NULL,
    createdutc   TEXT NOT NULL,
    updatedutc   TEXT NOT NULL,
    CHECK (roleband IN ('SPINE','RESEARCH','ENGINE','MATERIAL','GOV','APP')),
    CHECK (lanedefault IN ('RESEARCH','EXPPROD','PROD')),
    UNIQUE (filename, destination, repotarget)
);

INSERT OR IGNORE INTO econetfileindex (
    filename,
    destination,
    repotarget,
    roleband,
    lanedefault,
    description,
    createdutc,
    updatedutc
) VALUES (
    'db_ecocybo_machinery.sql',
    'eco_restoration_shard/sql/ecocybo/db_ecocybo_machinery.sql',
    'eco_restoration_shard',
    'RESEARCH',
    'RESEARCH',
    'Cyboquatic eco-machinery evidence spine (blast radius, workloads, eco scores).',
    datetime('now'),
    datetime('now')
);
