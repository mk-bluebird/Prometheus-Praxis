-- db/db_cyboquatic_governance_2026v1.sql
-- Cyboquatic carbon-negative + restoration-positive governance tables and views,
-- aligned with the existing governance spine and LaneStatusShard extensions.[file:6]

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. CyboquaticEcoPlot extension table
--    Adds energy, ecoperjoule, and carbon-negative flags per node window.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cyboquaticecoplot (
    nodeid            TEXT    NOT NULL,
    shardid           TEXT    NOT NULL,
    region            TEXT    NOT NULL,
    window_start_utc  INTEGER NOT NULL,
    window_end_utc    INTEGER NOT NULL,
    energy_joules     REAL    NOT NULL,
    work_units        REAL    NOT NULL,
    ecoperjoule       REAL    NOT NULL,
    carbonnegativeok  INTEGER NOT NULL CHECK (carbonnegativeok IN (0,1)),
    created_utc       INTEGER NOT NULL DEFAULT (strftime('%s','now')),
    PRIMARY KEY (nodeid, shardid, window_start_utc, window_end_utc)
);

CREATE INDEX IF NOT EXISTS idx_cybo_ecoplot_region
    ON cyboquaticecoplot(region, window_end_utc);

----------------------------------------------------------------------
-- 2. Cyboquatic restoration metrics table
--    Radius, mass window, and restoration-ok flag per node.[file:6]
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cyboquaticrestoration (
    nodeid               TEXT    NOT NULL,
    shardid              TEXT    NOT NULL,
    region               TEXT    NOT NULL,
    window_start_utc     INTEGER NOT NULL,
    window_end_utc       INTEGER NOT NULL,
    restorationradius_m  REAL    NOT NULL,
    deltamasswindow_kg   REAL    NOT NULL,
    deltakarma_window    REAL    NOT NULL,
    restorationok        INTEGER NOT NULL CHECK (restorationok IN (0,1)),
    created_utc          INTEGER NOT NULL DEFAULT (strftime('%s','now')),
    PRIMARY KEY (nodeid, shardid, window_start_utc, window_end_utc)
);

CREATE INDEX IF NOT EXISTS idx_cybo_restoration_region
    ON cyboquaticrestoration(region, window_end_utc);

----------------------------------------------------------------------
-- 3. LaneStatusShard augmentation: view wiring
--    Adds carbonnegativeok/restorationok flags onto the canonical lane view
--    based on latest Cyboquatic windows.[file:6]
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vcyboquaticecoperjoule AS
SELECT
    e.nodeid,
    e.shardid,
    e.ecoperjoule,
    e.carbonnegativeok,
    r.restorationok
FROM cyboquaticecoplot AS e
JOIN cyboquaticrestoration AS r
  ON e.nodeid = r.nodeid
 AND e.shardid = r.shardid
 AND e.window_end_utc = r.window_end_utc;

-- The underlying lanestatusshard table is assumed to exist in the spine DB.[file:6]
-- We define a view that overlays Cyboquatic flags into lane admissibility.

CREATE VIEW IF NOT EXISTS vlaneadmissibility AS
SELECT
    ls.shardid,
    ls.region,
    ls.lane,
    ls.verdict,
    ls.kerk,
    ls.kere,
    ls.kerr,
    ls.residualvt,
    ls.maxstalenesshours,
    ls.expiresutc,
    COALESCE(c.carbonnegativeok, 0) AS carbonnegativeok,
    COALESCE(c.restorationok, 0)     AS restorationok
FROM lanestatusshard AS ls
LEFT JOIN vcyboquaticecoperjoule AS c
  ON ls.shardid = c.shardid;

----------------------------------------------------------------------
-- 4. Cyboquatic governance eco-impact scoring (read-only view)
--    Summarizes ecoperjoule and restoration metrics with KER.[file:6]
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vcyboquaticrestorationnodes AS
SELECT
    c.nodeid,
    c.shardid,
    c.region,
    c.ecoperjoule,
    c.carbonnegativeok,
    r.restorationradius_m,
    r.deltamasswindow_kg,
    r.deltakarma_window,
    r.restorationok
FROM vcyboquaticecoperjoule AS c
JOIN cyboquaticrestoration AS r
  ON c.nodeid = r.nodeid
 AND c.shardid = r.shardid;

----------------------------------------------------------------------
-- 5. EcoNet governance spine bindings (non-actuating)
--    Lightweight catalog rows that make these tables discoverable by agents.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cyboquaticgovernanceshard (
    shardname    TEXT PRIMARY KEY,
    schemaversion TEXT NOT NULL,
    repotarget   TEXT NOT NULL,
    description  TEXT NOT NULL,
    created_utc  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

INSERT OR IGNORE INTO cyboquaticgovernanceshard (
    shardname, schemaversion, repotarget, description
) VALUES
    ('CyboquaticGovernance2026v1',
     '1.0',
     'eco_restoration_shard',
     'Cyboquatic carbon-negative and restoration-positive governance tables (CyboquaticEcoPlot, restoration windows, and lane admissibility overlays).');
