-- filename: eco_restoration_shard/db/db_steward_ecowealth_statement.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS StewardEcoWealthStatement (
    statementid INTEGER PRIMARY KEY AUTOINCREMENT,
    stewarddid TEXT NOT NULL,
    region TEXT NOT NULL,
    lane TEXT NOT NULL CHECK (lane IN ('RESEARCH','EXP','PROD')),
    windowstartutc TEXT NOT NULL,
    windowendutc TEXT NOT NULL,
    kmean REAL NOT NULL,
    kmin REAL NOT NULL,
    kmax REAL NOT NULL,
    emean REAL NOT NULL,
    emin REAL NOT NULL,
    emax REAL NOT NULL,
    rmean REAL NOT NULL,
    rmax REAL NOT NULL,
    vtmaxwindow REAL NOT NULL,
    kdelta REAL NOT NULL,
    edelta REAL NOT NULL,
    rdelta REAL NOT NULL,
    -- EcoUnit components and multipliers
    ecounitraw REAL NOT NULL,
    mkeducation REAL NOT NULL,
    deltaephys REAL NOT NULL,
    ecounitaftereducation REAL NOT NULL,
    representationweight REAL NOT NULL,
    ecounitfinal REAL NOT NULL,
    -- Governance context
    ecowealthkernelid TEXT NOT NULL,
    planecontractid TEXT NOT NULL,
    corridorsetid TEXT NOT NULL,
    lanepolicyid TEXT NOT NULL,
    -- Provenance and cryptographic commitments
    vshardkerwindowhash TEXT NOT NULL,
    shardlisthash TEXT NOT NULL,
    evidencehex TEXT NOT NULL,
    signingdid TEXT NOT NULL,
    createdutc TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_sews_steward_window
    ON StewardEcoWealthStatement (stewarddid, windowendutc DESC);

CREATE INDEX IF NOT EXISTS idx_sews_region_window
    ON StewardEcoWealthStatement (region, windowendutc DESC);

CREATE INDEX IF NOT EXISTS idx_sews_lane_region
    ON StewardEcoWealthStatement (lane, region, windowendutc DESC);

CREATE INDEX IF NOT EXISTS idx_sews_kernel
    ON StewardEcoWealthStatement (ecowealthkernelid, corridorsetid, lanepolicyid);
