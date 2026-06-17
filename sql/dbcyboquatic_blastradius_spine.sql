-- filename: dbcyboquatic_blastradius_spine.sql
-- destination: ecorestorationshard/sql/dbcyboquatic_blastradius_spine.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Cyboquatic blast-radius link table (non-actuating diagnostics)
--    Mirrors prior blastradiuslink patterns but scoped to Cyboquatic
--    nodes and eco-restorative industrial machinery.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_blastradius_link (
    linkid        INTEGER PRIMARY KEY AUTOINCREMENT,
    sourcetype    TEXT NOT NULL CHECK (sourcetype IN ('REPO','SCHEMA','PARTICLE','SHARD','MACHINE')),
    sourceid      TEXT NOT NULL,
    targettype    TEXT NOT NULL CHECK (targettype IN ('NODE','MACHINE','MATERIAL','REGION','CHANNEL')),
    targetid      TEXT NOT NULL,
    impacttype    TEXT NOT NULL CHECK (impacttype IN ('ENERGY','CARBON','MATERIALS','BIODIVERSITY','WATER','DATAQUALITY','GOVERNANCE')),
    impactscore   REAL NOT NULL,   -- 0..1 fraction of corridor width influenced
    vtsensitivity REAL,            -- change in Vt for diagnostics, dimensionless
    notes         TEXT,
    createdutc    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE INDEX IF NOT EXISTS idx_cybo_blast_source
    ON cybo_blastradius_link (sourcetype, sourceid, impacttype);

CREATE INDEX IF NOT EXISTS idx_cybo_blast_target
    ON cybo_blastradius_link (targettype, targetid, impacttype);

----------------------------------------------------------------------
-- 2. Cyboquatic eco-workload ledger (energy/carbon/materials/biodiv)
--    Read-only diagnostics; decisions are recorded, not executed here.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_workload_ledger (
    ledgerid       INTEGER PRIMARY KEY AUTOINCREMENT,
    nodeid         TEXT NOT NULL,
    machineid      TEXT NOT NULL,  -- Cyboquatic machine or tray line id
    channel        TEXT NOT NULL CHECK (channel IN ('energy','carbon','materials','biodiversity','water')),
    ereq_j         REAL NOT NULL,  -- requested energy in Joules
    esurplus_j     REAL NOT NULL,  -- surplus (>=0) in Joules
    rcarbon        REAL,           -- 0..1 carbon risk coord
    rbiodiv        REAL,           -- 0..1 biodiversity risk coord
    rmaterials     REAL,           -- 0..1 materials toxicity / waste coord
    rwater         REAL,           -- 0..1 water risk coord (e.g. contamination)
    vt_before      REAL NOT NULL,  -- Lyapunov residual before decision
    vt_after       REAL NOT NULL,  -- Lyapunov residual after decision
    decision       TEXT NOT NULL CHECK (decision IN ('ACCEPT','REJECT','REROUTE')),
    lane           TEXT NOT NULL CHECK (lane IN ('RESEARCH','EXPPROD','PROD')),
    region         TEXT NOT NULL,  -- e.g. 'Phoenix-AZ'
    timestamputc   TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE INDEX IF NOT EXISTS idx_cybo_workload_node_time
    ON cybo_workload_ledger (nodeid, timestamputc);

CREATE INDEX IF NOT EXISTS idx_cybo_workload_machine_time
    ON cybo_workload_ledger (machineid, timestamputc);

CREATE INDEX IF NOT EXISTS idx_cybo_workload_region_lane
    ON cybo_workload_ledger (region, lane);

----------------------------------------------------------------------
-- 3. View: node-level blast radius summary for Cyboquatic nodes
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_cybo_node_blastradius AS
SELECT
    br.targetid          AS nodeid,
    br.impacttype        AS impacttype,
    SUM(br.impactscore)  AS impactscore_sum,
    AVG(COALESCE(br.vtsensitivity, 0.0)) AS vtsensitivity_mean,
    COUNT(*)             AS linkcount
FROM cybo_blastradius_link br
WHERE br.targettype = 'NODE'
GROUP BY br.targetid, br.impacttype;

----------------------------------------------------------------------
-- 4. View: workload window trends per node (always-improve diagnostics)
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_cybo_workload_window AS
SELECT
    wl.nodeid                              AS nodeid,
    MIN(wl.timestamputc)                   AS window_start_utc,
    MAX(wl.timestamputc)                   AS window_end_utc,
    SUM(wl.ereq_j)                         AS total_requests_j,
    SUM(wl.esurplus_j)                     AS total_surplus_j,
    SUM(CASE WHEN wl.decision = 'ACCEPT' THEN wl.ereq_j ELSE 0.0 END)
                                          AS accepted_requests_j,
    SUM(CASE WHEN wl.decision = 'REJECT' THEN wl.ereq_j ELSE 0.0 END)
                                          AS rejected_requests_j,
    SUM(CASE WHEN wl.decision = 'REROUTE' THEN wl.ereq_j ELSE 0.0 END)
                                          AS rerouted_requests_j,
    AVG(wl.vt_before)                      AS mean_vt_before,
    AVG(wl.vt_after)                       AS mean_vt_after,
    AVG(wl.vt_after - wl.vt_before)        AS mean_delta_vt,
    AVG(wl.rcarbon)                        AS mean_rcarbon,
    AVG(wl.rbiodiv)                        AS mean_rbiodiv,
    AVG(wl.rmaterials)                     AS mean_rmaterials,
    AVG(wl.rwater)                         AS mean_rwater,
    CAST(
        SUM(CASE WHEN wl.decision = 'ACCEPT' THEN 1 ELSE 0 END)
        AS REAL
    ) / NULLIF(COUNT(*), 0)                AS accept_fraction
FROM cybo_workload_ledger wl
GROUP BY wl.nodeid;
