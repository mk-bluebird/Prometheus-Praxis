-- Stub Cyboquatic tables and test data for cdylib/FFI harness validation.
-- Non-actuating, diagnostics only.

PRAGMA foreign_keys = ON;

-- Existing blast-radius and workload tables (aligned with spine pattern).
CREATE TABLE IF NOT EXISTS blastradiuslink (
    linkid        INTEGER PRIMARY KEY AUTOINCREMENT,
    sourcetype    TEXT NOT NULL CHECK (sourcetype IN ('REPO','SCHEMA','PARTICLE','SHARD','FILE','NODE')),
    sourceid      TEXT NOT NULL,
    targettype    TEXT NOT NULL CHECK (targettype IN ('NODE','SHARD','MACHINE','MATERIAL','REGION')),
    targetid      TEXT NOT NULL,
    impacttype    TEXT NOT NULL,
    impactscore   REAL NOT NULL,
    vtsensitivity REAL,
    notes         TEXT
);

CREATE TABLE IF NOT EXISTS cyboworkloadledger (
    ledgerid      INTEGER PRIMARY KEY AUTOINCREMENT,
    shardid       TEXT NOT NULL,
    variantid     TEXT NOT NULL,
    nodeid        TEXT NOT NULL,
    channel       TEXT NOT NULL CHECK (channel IN ('energy','carbon','materials','biodiversity')),
    ereqj         REAL NOT NULL,
    esurplusj     REAL NOT NULL,
    rcarbon       REAL,
    rbiodiv       REAL,
    vtbefore      REAL NOT NULL,
    vtafter       REAL NOT NULL,
    decision      TEXT NOT NULL CHECK (decision IN ('ACCEPT','REJECT','REROUTE')),
    timestamputc  TEXT NOT NULL
);

-- Cyboquatic EcoPlot table.
CREATE TABLE IF NOT EXISTS CyboquaticEcoPlot (
    nodeid            TEXT NOT NULL,
    basinid           TEXT NOT NULL,
    region            TEXT NOT NULL,
    lane              TEXT NOT NULL CHECK (lane IN ('RESEARCH','EXPPROD','PROD')),
    windowstartutc    TEXT NOT NULL,
    windowendutc      TEXT NOT NULL,
    energyjoules      REAL NOT NULL CHECK (energyjoules >= 0.0),
    ecoperjoule       REAL NOT NULL CHECK (ecoperjoule >= 0.0),
    carbonnegativeok  INTEGER NOT NULL CHECK (carbonnegativeok IN (0,1)),
    vtcontrib         REAL NOT NULL DEFAULT 0.0,
    kscore            REAL NOT NULL CHECK (kscore BETWEEN 0.0 AND 1.0),
    escore            REAL NOT NULL CHECK (escore BETWEEN 0.0 AND 1.0),
    rscore            REAL NOT NULL CHECK (rscore BETWEEN 0.0 AND 1.0)
);

-- Cyboquatic RestorationSurface table.
CREATE TABLE IF NOT EXISTS CyboquaticRestorationSurface (
    nodeid               TEXT NOT NULL,
    windowstartutc       TEXT NOT NULL,
    windowendutc         TEXT NOT NULL,
    restorationradiusm   REAL NOT NULL CHECK (restorationradiusm >= 0.0),
    restorationradiushours REAL NOT NULL CHECK (restorationradiushours >= 0.0),
    deltamasswindowkg    REAL NOT NULL,
    deltakarmawindow     REAL NOT NULL,
    gwriskmax            REAL NOT NULL CHECK (gwriskmax BETWEEN 0.0 AND 1.0),
    restorationok        INTEGER NOT NULL CHECK (restorationok IN (0,1))
);

-- Test data for a Phoenix Cyboquatic node.

INSERT INTO blastradiuslink (
    sourcetype, sourceid, targettype, targetid,
    impacttype, impactscore, vtsensitivity, notes
) VALUES
    ('NODE', 'PHX-CYBO-NODE-01', 'REGION', 'Phoenix-AZ',
     'HYDRAULIC', 0.40, -0.03, 'Hydraulic coupling to Phoenix canals'),
    ('NODE', 'PHX-CYBO-NODE-01', 'REGION', 'Phoenix-AZ',
     'ENERGY', 0.30, -0.02, 'Energy routing improves eco-per-joule'),
    ('NODE', 'PHX-CYBO-NODE-01', 'MATERIAL', 'FLOWVAC-BATCH-2026-01',
     'CARBON', 0.25, -0.01, 'Biodegradable substrate is carbon-negative');

INSERT INTO cyboworkloadledger (
    shardid, variantid, nodeid, channel,
    ereqj, esurplusj, rcarbon, rbiodiv,
    vtbefore, vtafter, decision, timestamputc
) VALUES
    ('CyboVariant-42', 'CyboVariant-42', 'PHX-CYBO-NODE-01', 'energy',
     500.0, 5000.0, 0.20, 0.10,
     0.40, 0.395, 'ACCEPT', '2026-01-15T12:00:00Z'),
    ('CyboVariant-99', 'CyboVariant-99', 'PHX-CYBO-NODE-01', 'carbon',
     200.0, 4800.0, 0.18, 0.09,
     0.395, 0.392, 'ACCEPT', '2026-01-15T12:05:00Z');

INSERT INTO CyboquaticEcoPlot (
    nodeid, basinid, region, lane,
    windowstartutc, windowendutc,
    energyjoules, ecoperjoule, carbonnegativeok,
    vtcontrib, kscore, escore, rscore
) VALUES
    (
        'PHX-CYBO-NODE-01',
        'Phoenix-Basin-1',
        'Phoenix-AZ',
        'EXPPROD',
        '2026-01-01T00:00:00Z',
        '2026-01-31T23:59:59Z',
        5.0e6,
        0.85,
        1,
        0.40,
        0.94,
        0.91,
        0.12
    );

INSERT INTO CyboquaticRestorationSurface (
    nodeid,
    windowstartutc,
    windowendutc,
    restorationradiusm,
    restorationradiushours,
    deltamasswindowkg,
    deltakarmawindow,
    gwriskmax,
    restorationok
) VALUES
    (
        'PHX-CYBO-NODE-01',
        '2026-01-01T00:00:00Z',
        '2026-01-31T23:59:59Z',
        1500.0,
        24.0,
        -1200.0,
        0.35,
        0.25,
        1
    );
