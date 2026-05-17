-- filename: db/db_basetables.sql
-- destination: Eco-Fort/db/db_basetables.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. shardinstance: shard-level KER and residual anchors
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS shardinstance (
    shardid            INTEGER PRIMARY KEY AUTOINCREMENT,
    reponame           TEXT NOT NULL,
    nodeid             TEXT NOT NULL,
    region             TEXT NOT NULL,
    lane               TEXT NOT NULL,  -- RESEARCH, EXPPROD, PROD
    medium             TEXT,           -- e.g. HYDROPHOENIX, GRIDENERGY
    tsstartutc         TEXT NOT NULL,
    tsendutc           TEXT NOT NULL,
    planecontractid    INTEGER REFERENCES planeweightscontract(contractid)
                         ON DELETE SET NULL,
    vtmax              REAL NOT NULL,  -- Lyapunov residual in [0, +inf)
    kmetric            REAL NOT NULL CHECK (kmetric BETWEEN 0.0 AND 1.0),
    emetric            REAL NOT NULL CHECK (emetric BETWEEN 0.0 AND 1.0),
    rmetric            REAL NOT NULL CHECK (rmetric BETWEEN 0.0 AND 1.0),
    rtopology          REAL CHECK (rtopology IS NULL OR (rtopology BETWEEN 0.0 AND 1.0)),
    wtopology          REAL CHECK (wtopology IS NULL OR (wtopology >= 0.0)),
    kerdeployable      INTEGER NOT NULL DEFAULT 0 CHECK (kerdeployable IN (0,1)),
    evidencehex        TEXT,
    signingdid         TEXT,
    createdutc         TEXT NOT NULL,
    updatedutc         TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_shardinstance_region_lane
    ON shardinstance (region, lane);

CREATE INDEX IF NOT EXISTS idx_shardinstance_planecontract
    ON shardinstance (planecontractid);

-------------------------------------------------------------------------------
-- 2. residualkernel: contracts and kernel codes for V(t)
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS residualkernel (
    kernelid        INTEGER PRIMARY KEY AUTOINCREMENT,
    kernelcode      TEXT NOT NULL UNIQUE, -- e.g. ecosafety.Vt.core2026v1
    planecontractid INTEGER NOT NULL
        REFERENCES planeweightscontract(contractid)
        ON DELETE CASCADE,
    description     TEXT,
    frozen          INTEGER NOT NULL DEFAULT 0 CHECK (frozen IN (0,1)),
    createdutc      TEXT NOT NULL,
    updatedutc      TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_residualkernel_contract
    ON residualkernel (planecontractid);

-------------------------------------------------------------------------------
-- 3. residualterm: per-coordinate weights (alpha_j) for a kernel
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS residualterm (
    kernelid    INTEGER NOT NULL
        REFERENCES residualkernel(kernelid)
        ON DELETE CASCADE,
    coordid     TEXT NOT NULL,           -- e.g. 'r_energy', 'r_topology'
    alpha       REAL NOT NULL CHECK (alpha >= 0.0),
    nonoffset   INTEGER NOT NULL DEFAULT 0 CHECK (nonoffset IN (0,1)),
    PRIMARY KEY (kernelid, coordid)
);

CREATE INDEX IF NOT EXISTS idx_residualterm_kernel
    ON residualterm (kernelid);

-------------------------------------------------------------------------------
-- 4. shardriskcoord: per-shard risk coordinates r_j
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS shardriskcoord (
    shardid    INTEGER NOT NULL
        REFERENCES shardinstance(shardid)
        ON DELETE CASCADE,
    coordid    TEXT NOT NULL,           -- matches residualterm.coordid
    rvalue     REAL NOT NULL CHECK (rvalue BETWEEN 0.0 AND 1.0),
    PRIMARY KEY (shardid, coordid)
);

CREATE INDEX IF NOT EXISTS idx_shardriskcoord_coord
    ON shardriskcoord (coordid);

-------------------------------------------------------------------------------
-- 5. topologyriskmetric: topology audit results per shard
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS topologyriskmetric (
    topologyid   INTEGER PRIMARY KEY AUTOINCREMENT,
    shardid      INTEGER NOT NULL
        REFERENCES shardinstance(shardid)
        ON DELETE CASCADE,
    itopology    REAL NOT NULL,                     -- raw inconsistency index
    rtopology    REAL NOT NULL CHECK (rtopology BETWEEN 0.0 AND 1.0),
    nmissing     INTEGER NOT NULL DEFAULT 0 CHECK (nmissing >= 0),
    nmislabel    INTEGER NOT NULL DEFAULT 0 CHECK (nmislabel >= 0),
    auditrunid   INTEGER,                           -- link to topologyauditrun if present
    timestamputc TEXT NOT NULL,
    UNIQUE (shardid, timestamputc)
);

CREATE INDEX IF NOT EXISTS idx_topologyriskmetric_shard
    ON topologyriskmetric (shardid);

CREATE INDEX IF NOT EXISTS idx_topologyriskmetric_rtopology
    ON topologyriskmetric (rtopology);
