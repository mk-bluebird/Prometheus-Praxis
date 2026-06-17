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
    csup            REAL NOT NULL,  -- output of JurisdictionSupremumKernel
    hazard_weight   REAL NOT NULL,
    kn              REAL NOT NULL,
    ecoimpactscore  REAL NOT NULL,
    nanokarmabytes  REAL NOT NULL,  -- NanoKarmaKernel
    evidencehex     TEXT NOT NULL,
    createdutc      TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_ceim_node_contaminant
    ON ceim_xj_node_result(nodeid, contaminant, windowend);
