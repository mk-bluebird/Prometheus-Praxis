-- path: db/db_cyboquatic_machinery_spine.sql
-- repo: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Machinery registry (non-actuating identity surface)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_machinery (
    machine_id      TEXT PRIMARY KEY,   -- logical ID, not IP/MAC
    kind            TEXT NOT NULL,      -- 'PUMP','SCREEN','BLOWER','UF_TRAIN','SOIL_WASH','MBR','ESS'
    vendor_model    TEXT NOT NULL,      -- human-readable model
    region          TEXT NOT NULL,      -- e.g. 'Phoenix-AZ'
    lane            TEXT NOT NULL,      -- RESEARCH | PILOT | PRODUCTION
    ecosafety_shard TEXT NOT NULL,      -- ALN shard name governing corridors
    roh_lane_max    REAL NOT NULL,      -- 0..1 lane-specific RoH ceiling (≤ global 0.30)
    created_utc     TEXT NOT NULL,
    updated_utc     TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_cybo_machinery_region
    ON cybo_machinery (region);

CREATE INDEX IF NOT EXISTS idx_cybo_machinery_kind_lane
    ON cybo_machinery (kind, lane);

----------------------------------------------------------------------
-- 2. Blast-radius links (machinery → city / eco surfaces)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_machinery_blastradius (
    link_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    machine_id     TEXT NOT NULL,
    target_type    TEXT NOT NULL CHECK (target_type IN (
        'NODE','REGION','MATERIAL','AQUIFER','RIVER_REACH'
    )),
    target_id      TEXT NOT NULL,
    impact_type    TEXT NOT NULL CHECK (impact_type IN (
        'HYDRAULIC','ENERGY','CARBON','BIODIVERSITY','MATERIAL','DATAQUALITY'
    )),
    impact_score   REAL NOT NULL,  -- 0..1, fraction of corridor thickness influenced
    vt_sensitivity REAL,           -- approximate ΔV_t attributable to this link
    notes          TEXT,
    created_utc    TEXT NOT NULL,
    updated_utc    TEXT NOT NULL,
    FOREIGN KEY (machine_id) REFERENCES cybo_machinery(machine_id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    UNIQUE (machine_id, target_type, target_id, impact_type)
);

CREATE INDEX IF NOT EXISTS idx_cybo_mach_blast_machine
    ON cybo_machinery_blastradius (machine_id, impact_type);

CREATE INDEX IF NOT EXISTS idx_cybo_mach_blast_target
    ON cybo_machinery_blastradius (target_type, target_id);

----------------------------------------------------------------------
-- 3. Workload energy / carbon / stability ledger (non-actuating)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_machinery_workload (
    workload_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    machine_id     TEXT NOT NULL,
    node_id        TEXT NOT NULL,          -- EcoNet or hydrology node
    region         TEXT NOT NULL,
    lane           TEXT NOT NULL,          -- RESEARCH | PILOT | PRODUCTION
    channel        TEXT NOT NULL CHECK (channel IN ('energy','carbon','water','waste','biota')),
    e_req_j        REAL NOT NULL,         -- requested energy (J)
    e_used_j       REAL NOT NULL,         -- actual used energy (J)
    carbon_kg      REAL,                  -- estimated CO₂e for this workload
    pollutant_mass_kg REAL,               -- PFBS / E.coli / etc removed
    vt_before      REAL NOT NULL,         -- Lyapunov V_t before
    vt_after       REAL NOT NULL,         -- Lyapunov V_t after
    roh_scalar     REAL NOT NULL,         -- RoH 0..1 for this workload
    decision       TEXT NOT NULL CHECK (decision IN ('ACCEPT','DERATE','REJECT')),
    timestamp_utc  TEXT NOT NULL,
    evidence_hex   TEXT,                  -- hex evidence chain
    signing_did    TEXT,                  -- Bostrom DID for governance decision
    FOREIGN KEY (machine_id) REFERENCES cybo_machinery(machine_id)
        ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_cybo_mach_workload_node_time
    ON cybo_machinery_workload (node_id, timestamp_utc);

CREATE INDEX IF NOT EXISTS idx_cybo_mach_workload_machine
    ON cybo_machinery_workload (machine_id, channel);

----------------------------------------------------------------------
-- 4. Aggregated blast-radius views for AI / EcoNet
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_cybo_mach_blastradius_agg AS
SELECT
    m.machine_id,
    m.kind,
    m.region,
    m.lane,
    MAX(CASE WHEN b.target_type = 'NODE'        THEN b.impact_score ELSE 0 END) AS max_node_radius,
    MAX(CASE WHEN b.target_type = 'REGION'      THEN b.impact_score ELSE 0 END) AS max_region_radius,
    MAX(CASE WHEN b.impact_type = 'ENERGY'      THEN b.impact_score ELSE 0 END) AS max_energy_radius,
    MAX(CASE WHEN b.impact_type = 'CARBON'      THEN b.impact_score ELSE 0 END) AS max_carbon_radius,
    MAX(CASE WHEN b.impact_type = 'BIODIVERSITY' THEN b.impact_score ELSE 0 END) AS max_biodiv_radius,
    SUM(COALESCE(b.vt_sensitivity,0.0))        AS vt_radius_sum
FROM cybo_machinery m
LEFT JOIN cybo_machinery_blastradius b
    ON m.machine_id = b.machine_id
GROUP BY m.machine_id;

----------------------------------------------------------------------
-- 5. Node-window eco / stability summary (for always-improve loops)
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_cybo_mach_node_window AS
SELECT
    node_id,
    region,
    MIN(timestamp_utc) AS window_start_utc,
    MAX(timestamp_utc) AS window_end_utc,
    SUM(e_req_j)       AS total_e_req_j,
    SUM(e_used_j)      AS total_e_used_j,
    SUM(carbon_kg)     AS total_carbon_kg,
    SUM(pollutant_mass_kg) AS total_pollutant_mass_kg,
    AVG(vt_before)     AS mean_vt_before,
    AVG(vt_after)      AS mean_vt_after,
    AVG(roh_scalar)    AS mean_roh_scalar,
    SUM(CASE WHEN decision = 'ACCEPT' THEN 1 ELSE 0 END)  AS accepts,
    SUM(CASE WHEN decision = 'DERATE' THEN 1 ELSE 0 END)  AS derates,
    SUM(CASE WHEN decision = 'REJECT' THEN 1 ELSE 0 END)  AS rejects
FROM cybo_machinery_workload
GROUP BY node_id, region;
