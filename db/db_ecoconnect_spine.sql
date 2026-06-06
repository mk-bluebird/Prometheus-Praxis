-- filename: db/db_ecoconnect_spine.sql
-- destination: eco_restoration_shard/db/db_ecoconnect_spine.sql
-- Purpose:
--   - Non-actuating eco-connectivity spine.
--   - Stores circuit-theory resistances, APSP distances, species corridors,
--     and edge-node telemetry/ML diagnostics.

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Landscape graph and circuit-theory parameters
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_landscape_node (
    node_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    region_code    TEXT NOT NULL,            -- e.g. 'Phoenix-AZ'
    geohex         TEXT NOT NULL,            -- hex cell id or tile code
    elevation_m    REAL,
    habitat_type   TEXT,                     -- e.g. 'riparian','upland','ag'
    notes          TEXT
);

CREATE TABLE IF NOT EXISTS eco_landscape_edge (
    edge_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    src_node_id    INTEGER NOT NULL REFERENCES eco_landscape_node(node_id)
                   ON DELETE CASCADE,
    dst_node_id    INTEGER NOT NULL REFERENCES eco_landscape_node(node_id)
                   ON DELETE CASCADE,
    r_xy           REAL NOT NULL,           -- pixel-level resistance R_xy
    symmetry       INTEGER NOT NULL CHECK (symmetry IN (0,1)),
    corridor_tag   TEXT,                    -- e.g. 'wolf_corridor','pollinator'
    created_utc    TEXT NOT NULL,
    UNIQUE (src_node_id, dst_node_id, corridor_tag)
);

CREATE INDEX IF NOT EXISTS idx_eco_landscape_edge_src
    ON eco_landscape_edge (src_node_id, corridor_tag);

CREATE INDEX IF NOT EXISTS idx_eco_landscape_edge_dst
    ON eco_landscape_edge (dst_node_id, corridor_tag);

-------------------------------------------------------------------------------
-- 2. APSP / effective-resistance summaries
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_apsp_distance (
    apsp_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    src_node_id    INTEGER NOT NULL REFERENCES eco_landscape_node(node_id)
                   ON DELETE CASCADE,
    dst_node_id    INTEGER NOT NULL REFERENCES eco_landscape_node(node_id)
                   ON DELETE CASCADE,
    algorithm      TEXT NOT NULL,           -- 'Dijkstra','FloydWarshall','GPU'
    distance       REAL NOT NULL,           -- effective path length
    r_eff          REAL,                    -- effective resistance Reff
    corridor_tag   TEXT,                    -- optional corridor filter
    computed_utc   TEXT NOT NULL,
    UNIQUE (src_node_id, dst_node_id, algorithm, corridor_tag)
);

CREATE INDEX IF NOT EXISTS idx_eco_apsp_src
    ON eco_apsp_distance (src_node_id, corridor_tag);

CREATE INDEX IF NOT EXISTS idx_eco_apsp_dst
    ON eco_apsp_distance (dst_node_id, corridor_tag);

-------------------------------------------------------------------------------
-- 3. Species-specific corridor metrics over the graph
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_species_corridor (
    corridor_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    species_code   TEXT NOT NULL,           -- e.g. 'canis_lupus','apis_mellifera'
    src_node_id    INTEGER NOT NULL REFERENCES eco_landscape_node(node_id)
                   ON DELETE CASCADE,
    dst_node_id    INTEGER NOT NULL REFERENCES eco_landscape_node(node_id)
                   ON DELETE CASCADE,
    p_dispersal    REAL NOT NULL,           -- 0..1 dispersal probability
    r_eff          REAL NOT NULL,           -- effective resistance for this pair
    corridor_rank  REAL NOT NULL,           -- importance in network (pinch score)
    bottleneck     INTEGER NOT NULL CHECK (bottleneck IN (0,1)),
    model_tag      TEXT NOT NULL,           -- e.g. 'Omniscape-2026v1'
    computed_utc   TEXT NOT NULL,
    UNIQUE (species_code, src_node_id, dst_node_id, model_tag)
);

CREATE INDEX IF NOT EXISTS idx_eco_species_corridor_species
    ON eco_species_corridor (species_code, corridor_rank DESC);

-------------------------------------------------------------------------------
-- 4. Resource-constrained edge node registry (telemetry devices)
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS edge_node (
    edge_id        TEXT PRIMARY KEY,        -- stable id, e.g. 'EDGE-PHX-FOG-001'
    region_code    TEXT NOT NULL,
    medium         TEXT NOT NULL,           -- 'water','air','soil','grid'
    hardware_class TEXT NOT NULL,           -- 'RP2040','ESP32','FPGA','ASIC'
    cpu_mhz        REAL,
    ram_kb         INTEGER,
    battery_mah    INTEGER,
    neuromorphic   INTEGER NOT NULL CHECK (neuromorphic IN (0,1)),
    notes          TEXT
);

-------------------------------------------------------------------------------
-- 5. Edge telemetry windows and ML diagnostics
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS edge_telemetry_window (
    window_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    edge_id        TEXT NOT NULL REFERENCES edge_node(edge_id) ON DELETE CASCADE,
    t_start_utc    TEXT NOT NULL,
    t_end_utc      TEXT NOT NULL,
    lane           TEXT NOT NULL CHECK (lane IN ('RESEARCH','EXPPROD','PROD')),
    battery_drop_pct REAL,                  -- % drop in window
    cpu_util_avg   REAL,
    radio_duty_avg REAL,
    samples_count  INTEGER NOT NULL,
    eco_plane      TEXT NOT NULL,           -- 'energy','hydraulics','biology'
    k_metric       REAL,                    -- local K over window
    e_metric       REAL,                    -- local E
    r_metric       REAL,                    -- local R
    vt_max         REAL,                    -- max residual in window
    ker_deployable INTEGER NOT NULL DEFAULT 0
                   CHECK (ker_deployable IN (0,1))
);

CREATE INDEX IF NOT EXISTS idx_edge_telemetry_edge_time
    ON edge_telemetry_window (edge_id, t_start_utc);

-------------------------------------------------------------------------------
-- 6. Edge ML model variants and performance
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS edge_model_variant (
    model_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    name           TEXT NOT NULL,           -- 'ANN','KNN','LOF','CBLOF'
    supervision    TEXT NOT NULL,           -- 'supervised','unsupervised'
    description    TEXT,
    memory_profile TEXT NOT NULL,           -- 'low_ram','medium_ram','high_ram'
    best_context   TEXT                     -- freeform description
);

CREATE TABLE IF NOT EXISTS edge_model_score (
    score_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    model_id       INTEGER NOT NULL REFERENCES edge_model_variant(model_id)
                   ON DELETE CASCADE,
    edge_id        TEXT NOT NULL REFERENCES edge_node(edge_id) ON DELETE CASCADE,
    scenario_tag   TEXT NOT NULL,           -- 'solar_microgrid','soil_moisture'
    f1_score       REAL NOT NULL,
    latency_ms     REAL,
    cpu_util_avg   REAL,
    ram_kb_peak    INTEGER,
    t_evaluated_utc TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_edge_model_score_model
    ON edge_model_score (model_id, f1_score DESC);

-------------------------------------------------------------------------------
-- 7. Views: recommended model per edge & resource class
-------------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_edge_model_recommendation AS
SELECT
    s.edge_id,
    n.hardware_class,
    n.ram_kb,
    s.scenario_tag,
    v.name AS model_name,
    v.supervision,
    v.memory_profile,
    s.f1_score,
    s.latency_ms,
    s.cpu_util_avg
FROM edge_model_score s
JOIN edge_model_variant v ON v.model_id = s.model_id
JOIN edge_node n ON n.edge_id = s.edge_id
WHERE s.f1_score IS NOT NULL
ORDER BY s.edge_id, s.scenario_tag, s.f1_score DESC, s.latency_ms ASC;
