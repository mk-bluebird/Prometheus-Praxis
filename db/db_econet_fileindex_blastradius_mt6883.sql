-- filename db/db_econet_fileindex_blastradius_mt6883.sql
-- destination eco_restoration_shard/db/db_econet_fileindex_blastradius_mt6883.sql
-- repo-target github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- Minimal file index rows so agents know where this migration lives.

CREATE TABLE IF NOT EXISTS econetfileindex (
    fileindexid INTEGER PRIMARY KEY AUTOINCREMENT,
    filename TEXT NOT NULL,
    destination TEXT NOT NULL,
    repotarget TEXT NOT NULL,
    roleband TEXT NOT NULL,
    lanedefault TEXT NOT NULL,
    description TEXT NOT NULL,
    createdutc TEXT NOT NULL,
    updatedutc TEXT NOT NULL,
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
    'db_econet_constellation_blastradius_mt6883.sql',
    'eco_restoration_shard/db/db_econet_constellation_blastradius_mt6883.sql',
    'EcoNet',
    'SPINE',
    'RESEARCH',
    'Blast-radius zones, MT6883 RISK-chain, force-contribution ledger, and routing views for EcoNet constellation spine.',
    datetime('now'),
    datetime('now')
);
