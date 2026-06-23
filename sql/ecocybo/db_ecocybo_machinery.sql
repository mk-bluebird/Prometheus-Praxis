-- filename: db_ecocybo_machinery.sql
-- destination: eco_restoration_shard/sql/ecocybo/db_ecocybo_machinery.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Cyboquatic node registry: what machinery exists where
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_node (
    node_id            TEXT PRIMARY KEY,          -- e.g. PHX-CYBOQ-MAR-001
    region             TEXT NOT NULL,             -- e.g. Phoenix-AZ-Gila
    medium             TEXT NOT NULL,             -- water, soil, air
    node_family        TEXT NOT NULL,             -- MAR, FOG_CHANNEL, SUBSTRATE_TRAY
    description        TEXT,
    installed_utc      TEXT NOT NULL,
    retired_utc        TEXT
);

CREATE INDEX IF NOT EXISTS idx_cybo_node_region
    ON cybo_node(region, node_family);

----------------------------------------------------------------------
-- 2. Eco-plane weights and non-offsettable flags for nodes
--    (per-node view of PlaneWeightsShard2026v1)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_node_plane_weights (
    node_id                 TEXT NOT NULL REFERENCES cybo_node(node_id) ON DELETE CASCADE,
    w_energy                REAL NOT NULL,
    w_hydraulics            REAL NOT NULL,
    w_materials             REAL NOT NULL,
    w_carbon                REAL NOT NULL,
    w_biodiversity          REAL NOT NULL,
    w_dataquality           REAL NOT NULL,
    w_topology              REAL NOT NULL,
    nonoffsettable_carbon   INTEGER NOT NULL CHECK (nonoffsettable_carbon IN (0,1)),
    nonoffsettable_biodiv   INTEGER NOT NULL CHECK (nonoffsettable_biodiv IN (0,1)),
    PRIMARY KEY (node_id)
);

----------------------------------------------------------------------
-- 3. Blast-radius surfaces keyed by node_id (machinery → world impact)
--    This is a specialization of the global blastradiuslink.[file:16]
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_blast_radius (
    link_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    node_id        TEXT NOT NULL REFERENCES cybo_node(node_id) ON DELETE CASCADE,
    target_type    TEXT NOT NULL CHECK (target_type IN ('REGION','MATERIAL','AQUIFER','SEWER_SEGMENT')),
    target_id      TEXT NOT NULL,
    impact_plane   TEXT NOT NULL CHECK (impact_plane IN (
                        'ENERGY','HYDRAULICS','MATERIALS','CARBON','BIODIVERSITY','DATAQUALITY','TOPOLOGY'
                    )),
    impact_score   REAL NOT NULL,       -- 0..1 normalized impact magnitude
    vt_sensitivity REAL,                -- partial dVt/d(plane) if known
    note           TEXT
);

CREATE INDEX IF NOT EXISTS idx_cybo_blast_node
    ON cybo_blast_radius(node_id, impact_plane, target_type);

CREATE INDEX IF NOT EXISTS idx_cybo_blast_target
    ON cybo_blast_radius(target_type, target_id, impact_plane);

----------------------------------------------------------------------
-- 4. Node workload ledger (per-event diagnostics)
--    This mirrors cyboworkloadledger but is scoped per-node.[file:16]
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_node_workload (
    ledger_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    node_id        TEXT NOT NULL REFERENCES cybo_node(node_id) ON DELETE CASCADE,
    workload_id    TEXT NOT NULL,    -- e.g. HydrologicalBufferPhoenix2026v1:tick
    channel        TEXT NOT NULL CHECK (channel IN (
                        'energy','carbon','materials','biodiversity','hydraulics'
                    )),
    e_req_j        REAL NOT NULL,    -- joules required
    e_surplus_j    REAL NOT NULL,    -- joules surplus or saved
    r_energy       REAL,             -- optional plane-specific risk
    r_carbon       REAL,
    r_materials    REAL,
    r_biodiv       REAL,
    r_hydraulics   REAL,
    vt_before      REAL NOT NULL,
    vt_after       REAL NOT NULL,
    decision       TEXT NOT NULL CHECK (decision IN ('ACCEPT','REJECT','REROUTE')),
    ingest_rcalib  REAL,             -- 0..1 data-quality calibration risk
    ingest_rsigma  REAL,             -- 0..1 uncertainty sigma
    d_combined     REAL,             -- overall data trust 0..1
    ts_utc         TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_cybo_workload_node_time
    ON cybo_node_workload(node_id, ts_utc);

CREATE INDEX IF NOT EXISTS idx_cybo_workload_node_channel
    ON cybo_node_workload(node_id, channel);

----------------------------------------------------------------------
-- 5. EcoRestorationScore per node (windowed summaries)
--    This is the high-level “industrial health” metric agents query.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_node_eco_score (
    score_id           INTEGER PRIMARY KEY AUTOINCREMENT,
    node_id            TEXT NOT NULL REFERENCES cybo_node(node_id) ON DELETE CASCADE,
    window_start_utc   TEXT NOT NULL,
    window_end_utc     TEXT NOT NULL,
    k_avg              REAL NOT NULL CHECK (k_avg BETWEEN 0.0 AND 1.0),
    e_avg              REAL NOT NULL CHECK (e_avg BETWEEN 0.0 AND 1.0),
    r_avg              REAL NOT NULL CHECK (r_avg BETWEEN 0.0 AND 1.0),
    vt_max             REAL NOT NULL CHECK (vt_max >= 0.0),
    eco_gain           REAL NOT NULL,          -- ecological benefit units
    energy_cost_kwh    REAL NOT NULL,          -- kWh over window
    carbon_delta_kg    REAL NOT NULL,          -- net kg CO2e (negative is good)
    materials_residue  REAL NOT NULL,          -- normalized 0..1 micro-residue risk
    biodiv_delta       REAL NOT NULL,          -- biodiversity score change
    eco_efficiency     REAL NOT NULL,          -- eco_gain / energy_cost_kwh when >0
    reward_class       TEXT NOT NULL CHECK (reward_class IN ('BASELINE','GREEN','GOLD')),
    eco_restorative    INTEGER NOT NULL CHECK (eco_restorative IN (0,1)), -- 1 if restorative
    created_utc        TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_cybo_score_node_window
    ON cybo_node_eco_score(node_id, window_start_utc, window_end_utc);

----------------------------------------------------------------------
-- 6. View: simple high-level blast-radius window per node
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_cybo_node_blast_aggregate AS
SELECT
    node_id,
    impact_plane,
    AVG(impact_score) AS avg_impact_score,
    MAX(impact_score) AS max_impact_score
FROM cybo_blast_radius
GROUP BY node_id, impact_plane;
