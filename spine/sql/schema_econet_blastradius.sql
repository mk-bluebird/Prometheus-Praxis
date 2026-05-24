-- filename: spine/sql/schema_econet_blastradius.sql
-- Purpose: Non-actuating blastradius and canal surfaces for cyboquatic machinery.
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS blastradius_link (
    link_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    source_type   TEXT    NOT NULL CHECK (source_type IN ('REPO','SCHEMA','PARTICLE','SHARD','FILE')),
    source_id     TEXT    NOT NULL,
    target_type   TEXT    NOT NULL CHECK (target_type IN ('NODE','SHARD','MACHINE','MATERIAL','REGION')),
    target_id     TEXT    NOT NULL,
    impact_type   TEXT    NOT NULL CHECK (
        impact_type IN ('HYDRAULIC','ENERGY','CARBON','BIODIVERSITY','MATERIAL','DATA_QUALITY','GOVERNANCE')
    ),
    impact_score  REAL    NOT NULL,   -- 0..1 normalized fraction of corridor width touched
    vt_sensitivity REAL,              -- approximate |ΔV_t| per unit change
    notes         TEXT
);

CREATE INDEX IF NOT EXISTS idx_blastradius_source
    ON blastradius_link (source_type, source_id, impact_type);

CREATE INDEX IF NOT EXISTS idx_blastradius_target
    ON blastradius_link (target_type, target_id, impact_type);

-- filename: spine/sql/schema_econet_energy_carbon_ledger.sql
-- Purpose: Non-actuating energy/carbon ledger for cyboquatic workloads.
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS workload_ledger (
    ledger_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    shard_id      INTEGER NOT NULL,
    variant_id    TEXT    NOT NULL,
    node_id       TEXT    NOT NULL,
    channel       TEXT    NOT NULL CHECK (channel IN ('energy','carbon','materials','biodiversity')),
    e_req_j       REAL    NOT NULL,    -- requested energy (J)
    e_surplus_j   REAL    NOT NULL,    -- surplus at dispatch (J)
    r_carbon      REAL,                -- normalized carbon risk coordinate
    r_biodiv      REAL,                -- normalized biodiversity risk
    vt_before     REAL    NOT NULL,
    vt_after      REAL    NOT NULL,
    decision      TEXT    NOT NULL CHECK (decision IN ('ACCEPT','REJECT','REROUTE')),
    timestamp_utc TEXT    NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_workload_node_time
    ON workload_ledger (node_id, timestamp_utc);

CREATE INDEX IF NOT EXISTS idx_workload_shard
    ON workload_ledger (shard_id, channel);
