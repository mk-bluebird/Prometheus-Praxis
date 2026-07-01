-- filename: db/db_cyboquatic_blastradius_spine.sql
-- destination: eco_restoration_shard/db/db_cyboquatic_blastradius_spine.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Core blastradius link table (machinery → impact surfaces)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS blastradius_link (
    link_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    source_type    TEXT    NOT NULL CHECK (source_type IN ('REPO','SCHEMA','PARTICLE','SHARD','FILE','MACHINE')),
    source_id      TEXT    NOT NULL,  -- shard_id, machine_id, file path, etc.
    target_type    TEXT    NOT NULL CHECK (target_type IN ('NODE','SHARD','MACHINE','MATERIAL','REGION')),
    target_id      TEXT    NOT NULL,  -- e.g. hydrological node, material batch, region code
    impact_type    TEXT    NOT NULL,  -- 'HYDRAULIC','ENERGY','CARBON','BIODIVERSITY','MATERIAL','DATAQUALITY','GOVERNANCE'
    impact_score   REAL    NOT NULL,  -- 0..1 fraction of corridor width influenced
    vt_sensitivity REAL,              -- approximate ΔV_t footprint (dimensionless)
    notes          TEXT,
    created_utc    TEXT    NOT NULL,
    updated_utc    TEXT    NOT NULL,
    UNIQUE (source_type, source_id, target_type, target_id, impact_type)
);

CREATE INDEX IF NOT EXISTS idx_blastradius_source
    ON blastradius_link (source_type, source_id, impact_type);

CREATE INDEX IF NOT EXISTS idx_blastradius_target
    ON blastradius_link (target_type, target_id, impact_type);

----------------------------------------------------------------------
-- 2. Cyboquatic workload energy / carbon ledger
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cyboquatic_workload_ledger (
    ledger_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    shard_id       TEXT    NOT NULL,  -- FK into ecosafety/EcoNet shardinstance.shard_id
    variant_id     TEXT    NOT NULL,  -- Cyboquatic variant or ALN particle version
    machine_id     TEXT    NOT NULL,  -- physical or logical machine identifier
    node_id        TEXT    NOT NULL,  -- hydrological / infrastructure node
    channel        TEXT    NOT NULL CHECK (channel IN ('energy','carbon','materials','biodiversity')),
    e_req_j        REAL    NOT NULL,  -- requested energy (J)
    e_surplus_j    REAL    NOT NULL,  -- surplus available energy at dispatch (J)
    r_carbon       REAL,              -- normalized carbon risk coordinate 0..1
    r_biodiv       REAL,              -- normalized biodiversity risk 0..1
    vt_before      REAL    NOT NULL,  -- Lyapunov residual before workload
    vt_after       REAL    NOT NULL,  -- Lyapunov residual after workload
    decision       TEXT    NOT NULL CHECK (decision IN ('ACCEPT','REJECT','REROUTE')),
    lane           TEXT    NOT NULL,  -- RESEARCH / EXPPROD / PROD
    region         TEXT    NOT NULL,  -- e.g. 'Phoenix-AZ'
    timestamp_utc  TEXT    NOT NULL,  -- ISO8601
    evidence_hex   TEXT,              -- hex of evidence chain
    signing_did    TEXT               -- Bostrom DID of governance decision
);

CREATE INDEX IF NOT EXISTS idx_cybo_workload_node_time
    ON cyboquatic_workload_ledger (node_id, timestamp_utc);

CREATE INDEX IF NOT EXISTS idx_cybo_workload_shard_channel
    ON cyboquatic_workload_ledger (shard_id, channel);

----------------------------------------------------------------------
-- 3. Views for blast radius per shard / machine
----------------------------------------------------------------------

-- Shard-level blast radius aggregated view
CREATE VIEW IF NOT EXISTS v_shard_blastradius AS
SELECT
    source_id          AS shard_id,
    MAX(CASE WHEN target_type = 'NODE'        THEN impact_score ELSE 0 END) AS max_node_radius,
    MAX(CASE WHEN target_type = 'MATERIAL'    THEN impact_score ELSE 0 END) AS max_material_radius,
    MAX(CASE WHEN impact_type = 'CARBON'      THEN impact_score ELSE 0 END) AS max_carbon_radius,
    MAX(CASE WHEN impact_type = 'BIODIVERSITY' THEN impact_score ELSE 0 END) AS max_biodiv_radius,
    SUM(COALESCE(vt_sensitivity, 0.0))        AS vt_radius_sum
FROM blastradius_link
WHERE source_type = 'SHARD'
GROUP BY source_id;

-- Machine-level blast radius aggregated view
CREATE VIEW IF NOT EXISTS v_machine_blastradius AS
SELECT
    source_id          AS machine_id,
    MAX(CASE WHEN target_type = 'NODE'        THEN impact_score ELSE 0 END) AS max_node_radius,
    MAX(CASE WHEN target_type = 'REGION'      THEN impact_score ELSE 0 END) AS max_region_radius,
    MAX(CASE WHEN impact_type = 'ENERGY'      THEN impact_score ELSE 0 END) AS max_energy_radius,
    MAX(CASE WHEN impact_type = 'CARBON'      THEN impact_score ELSE 0 END) AS max_carbon_radius,
    SUM(COALESCE(vt_sensitivity, 0.0))        AS vt_radius_sum
FROM blastradius_link
WHERE source_type = 'MACHINE'
GROUP BY source_id;

----------------------------------------------------------------------
-- 4. Always-improve corridor helper views (non-actuating)
----------------------------------------------------------------------

-- View: workload summary per node / region over a window
CREATE VIEW IF NOT EXISTS v_cybo_workload_node_window AS
SELECT
    node_id,
    region,
    MIN(timestamp_utc)                                AS window_start_utc,
    MAX(timestamp_utc)                                AS window_end_utc,
    SUM(e_req_j)                                      AS total_req_j,
    SUM(e_surplus_j)                                  AS total_surplus_j,
    AVG(vt_before)                                    AS mean_vt_before,
    AVG(vt_after)                                     AS mean_vt_after,
    AVG(CASE WHEN r_carbon IS NOT NULL  THEN r_carbon  END) AS mean_r_carbon,
    AVG(CASE WHEN r_biodiv IS NOT NULL  THEN r_biodiv  END) AS mean_r_biodiv,
    SUM(CASE WHEN decision = 'ACCEPT'  THEN 1 ELSE 0 END)  AS accepts,
    SUM(CASE WHEN decision = 'REJECT'  THEN 1 ELSE 0 END)  AS rejects,
    SUM(CASE WHEN decision = 'REROUTE' THEN 1 ELSE 0 END)  AS reroutes
FROM cyboquatic_workload_ledger
GROUP BY node_id, region;
