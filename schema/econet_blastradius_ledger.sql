-- filename: schema/econet_blastradius_ledger.sql
-- Purpose: Extend EcoNet discovery spine with blast-radius metadata and
--          cyboquatic workload energy/carbon ledger. Non-actuating only.

PRAGMA foreign_keys = ON;

-- Blast-radius link: how an abstract source (repo/schema/particle/shard/file)
-- influences physical or ecological targets (node, material, machine, region).
CREATE TABLE IF NOT EXISTS blastradiuslink (
    linkid        INTEGER PRIMARY KEY AUTOINCREMENT,
    sourcetype    TEXT NOT NULL CHECK (sourcetype IN ('REPO','SCHEMA','PARTICLE','SHARD','FILE')),
    sourceid      INTEGER NOT NULL,
    targettype    TEXT NOT NULL CHECK (targettype IN ('NODE','SHARD','MACHINE','MATERIAL','REGION')),
    targetid      TEXT NOT NULL,      -- nodeid, region code, material id, etc.
    impacttype    TEXT NOT NULL,      -- HYDRAULIC,ENERGY,CARBON,BIODIVERSITY,MATERIAL,DATAQUALITY,GOVERNANCE
    impactscore   REAL NOT NULL,      -- 0..1 fraction of corridor width influenced
    vtsensitivity REAL,               -- approximate ∂V/∂source (dimensionless)
    notes         TEXT
);

CREATE INDEX IF NOT EXISTS idx_blastradius_source
    ON blastradiuslink (sourcetype, sourceid, impacttype);

CREATE INDEX IF NOT EXISTS idx_blastradius_target
    ON blastradiuslink (targettype, targetid, impacttype);

-- Workload energy/carbon/biodiversity ledger for cyboquatic workloads.
-- This is a diagnostic ledger over executed or simulated workloads;
-- it never drives actuators directly.
CREATE TABLE IF NOT EXISTS workloadledger (
    ledgerid      INTEGER PRIMARY KEY AUTOINCREMENT,
    shardid       INTEGER NOT NULL REFERENCES shardinstance(shardid) ON DELETE CASCADE,
    variantid     TEXT    NOT NULL,   -- workload or algorithm variant id
    nodeid        TEXT    NOT NULL,
    channel       TEXT    NOT NULL CHECK (channel IN ('energy','carbon','materials','biodiversity')),
    ereq_j        REAL    NOT NULL,   -- requested energy (J)
    esurplus_j    REAL    NOT NULL,   -- surplus energy at dispatch (J)
    rcarbon       REAL,               -- normalized carbon risk (0..1)
    rbiodiv       REAL,               -- normalized biodiversity risk (0..1)
    vt_before     REAL    NOT NULL,   -- Lyapunov residual before workload
    vt_after      REAL    NOT NULL,   -- Lyapunov residual after workload
    decision      TEXT    NOT NULL CHECK (decision IN ('ACCEPT','REJECT','REROUTE')),
    timestamputc  TEXT    NOT NULL    -- ISO8601
);

CREATE INDEX IF NOT EXISTS idx_workload_node_time
    ON workloadledger (nodeid, timestamputc);

CREATE INDEX IF NOT EXISTS idx_workload_shard
    ON workloadledger (shardid, channel);

-- Convenience view: per-shard blast radius summary.
CREATE VIEW IF NOT EXISTS v_shard_blastradius AS
SELECT
    s.shardid,
    s.nodeid,
    s.region,
    s.lane,
    s.kmetric,
    s.emetric,
    s.rmetric,
    s.vtmax,
    b.impacttype,
    SUM(b.impactscore)      AS impactscore_sum,
    MAX(b.vtsensitivity)    AS vtsensitivity_max
FROM shardinstance AS s
LEFT JOIN blastradiuslink AS b
  ON b.sourcetype = 'SHARD'
 AND b.sourceid   = s.shardid
GROUP BY
    s.shardid,
    s.nodeid,
    s.region,
    s.lane,
    s.kmetric,
    s.emetric,
    s.rmetric,
    s.vtmax,
    b.impacttype;

-- Convenience view: lane-safe, carbon-negative candidates for always-improve agents.
CREATE VIEW IF NOT EXISTS v_lane_safe_carbon_negative AS
SELECT
    s.shardid,
    s.nodeid,
    s.region,
    s.lane,
    s.kmetric,
    s.emetric,
    s.rmetric,
    s.vtmax,
    w.variantid,
    w.ereq_j,
    w.esurplus_j,
    w.rcarbon,
    w.rbiodiv,
    w.vt_before,
    w.vt_after,
    w.decision
FROM shardinstance AS s
JOIN workloadledger AS w
  ON w.shardid = s.shardid
WHERE
    s.kerdeployable = 1
    AND s.lane IN ('RESEARCH','EXPPROD','PROD')
    AND w.decision = 'ACCEPT'
    AND w.rcarbon IS NOT NULL
    AND w.rcarbon <= 0.13   -- stays within low-risk carbon band
    AND w.vt_after <= w.vt_before;
