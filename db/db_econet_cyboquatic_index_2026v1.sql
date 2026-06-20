-- db/db_econet_cyboquatic_index_2026v1.sql
-- Cross-repo index for Cyboquatic industrial machinery shards in the EcoNet
-- constellation, designed to plug into existing econetrepoindex and
-- DefinitionRegistry patterns.[file:1][file:9]

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Cyboquatic machinery registry (non‑actuating)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS econet_cyboquatic_node (
    nodeid        TEXT PRIMARY KEY,
    repotarget    TEXT NOT NULL,
    shardid       TEXT NOT NULL,
    region        TEXT NOT NULL,
    basin         TEXT NOT NULL,
    medium        TEXT NOT NULL,      -- e.g. "water", "sediment"
    lane_default  TEXT NOT NULL,      -- RESEARCH/EXPPROD/PROD
    nonactuating  INTEGER NOT NULL CHECK (nonactuating IN (0,1)),
    did_owner     TEXT NOT NULL,
    description   TEXT NOT NULL,
    created_utc   TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE INDEX IF NOT EXISTS idx_cybo_node_region
    ON econet_cyboquatic_node(region, basin);

----------------------------------------------------------------------
-- 2. Binding to DefinitionRegistry for psychological continuity
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS econet_cyboquatic_definition_binding (
    nodeid        TEXT NOT NULL,
    defid        TEXT NOT NULL,
    role         TEXT NOT NULL,  -- e.g. "KER_KERNEL", "LANE_POLICY", "ECO_PLOT"
    PRIMARY KEY (nodeid, defid, role),
    FOREIGN KEY (nodeid) REFERENCES econet_cyboquatic_node(nodeid)
        ON DELETE CASCADE
);

----------------------------------------------------------------------
-- 3. Read-only view for AI/CI discovery
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS veconet_cyboquatic_nodes AS
SELECT
    n.nodeid,
    n.repotarget,
    n.shardid,
    n.region,
    n.basin,
    n.medium,
    n.lane_default,
    n.nonactuating,
    n.did_owner,
    n.description
FROM econet_cyboquatic_node AS n;
