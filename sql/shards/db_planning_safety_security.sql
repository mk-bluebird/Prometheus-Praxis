-- filename db_planning_safety_security.sql
-- destination eco_restoration_shard/sql/shards/db_planning_safety_security.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS planning_safety_security (
    rowid              INTEGER PRIMARY KEY AUTOINCREMENT,
    nodeid             TEXT NOT NULL,
    medium             TEXT NOT NULL,  -- air | water
    region             TEXT NOT NULL,
    twindowstart       TEXT NOT NULL,
    twindowend         TEXT NOT NULL,
    braw               REAL NOT NULL,  -- ESPD eco-benefit
    rraw               REAL NOT NULL,  -- ESPD risk
    dt                 REAL NOT NULL,  -- TrustDisplacementKernel output
    ki                 REAL NOT NULL,  -- Karma from NKK/identity shard
    ti                 REAL NOT NULL,  -- KarmaToleranceKernel output
    badj               REAL NOT NULL,  -- braw * dt
    safetyphase        TEXT NOT NULL,  -- FORBID | PILOT | DEPLOYABLE
    securityresponsecap TEXT NOT NULL, -- LOW | MEDIUM | HIGH
    evidencehex        TEXT NOT NULL,
    createdutc         TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_pss_node_time
    ON planning_safety_security(nodeid, twindowend);

CREATE INDEX IF NOT EXISTS idx_pss_region_medium
    ON planning_safety_security(region, medium, twindowend);
