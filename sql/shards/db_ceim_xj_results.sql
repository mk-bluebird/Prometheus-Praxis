-- filename db_ceim_xj_results.sql
-- destination eco_restoration_shard/sql/shards/db_ceim_xj_results.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS ceim_xj_node_result (
    resultid        INTEGER PRIMARY KEY AUTOINCREMENT,
    nodeid          TEXT NOT NULL,
    contaminant     TEXT NOT NULL,
    region          TEXT NOT NULL,
    windowstart     TEXT NOT NULL,
    windowend       TEXT NOT NULL,
    cin             REAL NOT NULL,
    cout            REAL NOT NULL,
    flow            REAL NOT NULL,
    cref            REAL NOT NULL,
    csup            REAL NOT NULL,   -- output of JurisdictionSupremumKernel
    hazard_weight   REAL NOT NULL,
    kn              REAL NOT NULL,
    ecoimpactscore  REAL NOT NULL,
    nanokarmabytes  REAL NOT NULL,   -- NanoKarmaKernel
    evidencehex     TEXT NOT NULL,
    createdutc      TEXT NOT NULL
);

-- Core node + contaminant + time index for CEIM traces
CREATE INDEX IF NOT EXISTS idx_ceim_node_contaminant
    ON ceim_xj_node_result (nodeid, contaminant, windowend);

-- Efficient lookups by contaminant and window end
CREATE INDEX IF NOT EXISTS idx_ceim_contaminant_windowend
    ON ceim_xj_node_result (contaminant, windowend);

-- Efficient lookups and grouping by region and contaminant
CREATE INDEX IF NOT EXISTS idx_ceim_region_contaminant
    ON ceim_xj_node_result (region, contaminant);

-- Efficient node-time scans regardless of contaminant
CREATE INDEX IF NOT EXISTS idx_ceim_node_windowend
    ON ceim_xj_node_result (nodeid, windowend);

-- Bostrom DID anchor for this shard
-- bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
