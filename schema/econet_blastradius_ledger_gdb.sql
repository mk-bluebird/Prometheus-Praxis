-- filename: schema/econet_blastradius_ledger_gdb.sql
-- destination: eco_restoration_shard/schema/econet_blastradius_ledger_gdb.sql
-- Purpose: Extend EcoNet spine with blast-radius, workload ledger,
--          and explicit GDB + restoration event-window structures,
--          all non-actuating and wired to SKO/GDB/eco_wealth semantics.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Blast-radius link and workload ledger (refined, GDB-aware)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS blastradiuslink (
    linkid        INTEGER PRIMARY KEY AUTOINCREMENT,
    sourcetype    TEXT NOT NULL CHECK (
                      sourcetype IN ('REPO','SCHEMA','PARTICLE','SHARD','FILE','GDB')
                  ),
    sourceid      TEXT NOT NULL,
    targettype    TEXT NOT NULL CHECK (
                      targettype IN ('NODE','SHARD','MACHINE','MATERIAL','REGION','GDB')
                  ),
    targetid      TEXT NOT NULL,   -- nodeid, region code, material id, gdbid, etc.
    impactplane   TEXT NOT NULL,   -- HYDRAULICS,CARBON,BIODIVERSITY,MATERIALS,TOPOLOGY,ENERGY,GOVERNANCE
    impactkind    TEXT NOT NULL CHECK (
                      impactkind IN ('STABILIZING','DESTABILIZING','NEUTRAL')
                  ),
    impactscore   REAL NOT NULL CHECK (impactscore >= 0.0 AND impactscore <= 1.0),
    vtsensitivity REAL,            -- approximate ∂V/∂source on this plane
    notes         TEXT
);

CREATE INDEX IF NOT EXISTS idx_blastradius_source_plane
    ON blastradiuslink (sourcetype, sourceid, impactplane);

CREATE INDEX IF NOT EXISTS idx_blastradius_target_plane
    ON blastradiuslink (targettype, targetid, impactplane);

CREATE INDEX IF NOT EXISTS idx_blastradius_gdb_target
    ON blastradiuslink (targettype, targetid);

CREATE TABLE IF NOT EXISTS workloadledger (
    ledgerid      INTEGER PRIMARY KEY AUTOINCREMENT,
    shardid       INTEGER NOT NULL REFERENCES shardinstance(shardid) ON DELETE CASCADE,
    gdbid         INTEGER,              -- optional link to logical GDB instance
    variantid     TEXT    NOT NULL,     -- workload / algorithm variant id
    nodeid        TEXT    NOT NULL,
    channel       TEXT    NOT NULL CHECK (
                      channel IN ('energy','carbon','materials','biodiversity')
                  ),
    ereq_j        REAL    NOT NULL,     -- requested energy (J)
    esurplus_j    REAL    NOT NULL,     -- surplus energy at dispatch (J)
    rcarbon       REAL,                 -- normalized carbon risk (0..1)
    rbiodiv       REAL,                 -- normalized biodiversity risk (0..1)
    vt_before     REAL    NOT NULL,     -- Lyapunov residual before workload
    vt_after      REAL    NOT NULL,     -- Lyapunov residual after workload
    decision      TEXT    NOT NULL CHECK (
                      decision IN ('ACCEPT','REJECT','REROUTE')
                  ),
    timestamputc  TEXT    NOT NULL      -- ISO8601
);

CREATE INDEX IF NOT EXISTS idx_workload_node_time
    ON workloadledger (nodeid, timestamputc);

CREATE INDEX IF NOT EXISTS idx_workload_shard_channel
    ON workloadledger (shardid, channel);

CREATE INDEX IF NOT EXISTS idx_workload_gdb
    ON workloadledger (gdbid);

----------------------------------------------------------------------
-- 2. GDB instances: geospatial-governance knowledge bundles
--    These are the concrete Gold Data Block payloads SKOs point to.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS gdb_instance (
    gdbid                INTEGER PRIMARY KEY AUTOINCREMENT,
    logicalname          TEXT NOT NULL,      -- e.g. "PHX-GILA-REACH-001"
    description          TEXT NOT NULL,
    region               TEXT NOT NULL,      -- matches shardinstance.region where applicable
    lane                 TEXT NOT NULL CHECK (lane IN ('RESEARCH','EXPPROD','PROD')),
    sizeclass            TEXT NOT NULL CHECK (sizeclass IN ('S','M','L','XL')),
    tierplacement        TEXT NOT NULL CHECK (tierplacement IN ('T0','T1','T2','T3')),
    mediumid             TEXT NOT NULL,      -- hardware/vault MediumId
    storagepath          TEXT NOT NULL,      -- datalake.storage_path reference
    pidle_w_per_tb       REAL NOT NULL,
    pactive_w_per_tb     REAL NOT NULL,
    pue                  REAL NOT NULL,
    carbon_kg_per_kwh    REAL NOT NULL,
    renewable_fraction   REAL NOT NULL CHECK (renewable_fraction >= 0.0 AND renewable_fraction <= 1.0),
    roh_floor            REAL NOT NULL,
    neurorights_compliant INTEGER NOT NULL CHECK (neurorights_compliant IN (0,1)),
    breach_history_score REAL NOT NULL,
    latency_band         TEXT NOT NULL,
    vt_snapshot          REAL NOT NULL,      -- representative V_t for this GDB
    eco_wealth_score     REAL NOT NULL,      -- instance-level EW (utility/carbon)
    created_utc          TEXT NOT NULL,
    updated_utc          TEXT NOT NULL,
    UNIQUE (logicalname, region)
);

CREATE INDEX IF NOT EXISTS idx_gdb_region_lane
    ON gdb_instance (region, lane);

CREATE INDEX IF NOT EXISTS idx_gdb_medium_tier
    ON gdb_instance (mediumid, tierplacement);

CREATE INDEX IF NOT EXISTS idx_gdb_eco_wealth
    ON gdb_instance (eco_wealth_score DESC);

----------------------------------------------------------------------
-- 3. SKO/GDB linkage: where SKO headers point for bytes and carbon
--    (this assumes an existing SKO table, e.g. skoinstance or similar.)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS sko_gdb_binding (
    bindingid       INTEGER PRIMARY KEY AUTOINCREMENT,
    skoid           INTEGER NOT NULL,      -- FK into SKO table in EcoNet spine
    gdbid           INTEGER NOT NULL REFERENCES gdb_instance(gdbid) ON DELETE CASCADE,
    role            TEXT NOT NULL CHECK (role IN ('PRIMARY','SHADOW','BACKUP')),
    bytes_fraction  REAL NOT NULL CHECK (bytes_fraction >= 0.0 AND bytes_fraction <= 1.0),
    roh_ceiling     REAL NOT NULL,         -- SKO-specific RoH ceiling
    neurorights_sensitive INTEGER NOT NULL CHECK (neurorights_sensitive IN (0,1)),
    created_utc     TEXT NOT NULL,
    updated_utc     TEXT NOT NULL,
    UNIQUE (skoid, gdbid)
);

CREATE INDEX IF NOT EXISTS idx_sko_gdb_sko
    ON sko_gdb_binding (skoid);

CREATE INDEX IF NOT EXISTS idx_sko_gdb_gdb
    ON sko_gdb_binding (gdbid);

----------------------------------------------------------------------
-- 4. Ecological-restoration event windows (bounded evaluation periods)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS restoration_event_window (
    windowid             INTEGER PRIMARY KEY AUTOINCREMENT,
    region               TEXT NOT NULL,
    label                TEXT NOT NULL,        -- e.g. "Phoenix-AZ – Gila reach – weekly"
    tstart_utc           TEXT NOT NULL,
    tend_utc             TEXT NOT NULL,
    lanescope            TEXT NOT NULL CHECK (lanescope IN ('RESEARCH','EXPPROD','PROD','ALL')),
    migration_budget_bytes INTEGER NOT NULL,   -- budget for GDB moves
    migration_budget_iops  INTEGER NOT NULL,
    carbon_budget_kg       REAL NOT NULL,
    roh_budget             REAL NOT NULL,      -- max allowed aggregate RoH increment
    created_utc            TEXT NOT NULL,
    updated_utc            TEXT NOT NULL,
    UNIQUE (region, tstart_utc, tend_utc, lanescope)
);

CREATE INDEX IF NOT EXISTS idx_restoration_window_region_time
    ON restoration_event_window (region, tstart_utc, tend_utc);

----------------------------------------------------------------------
-- 5. Window–GDB metrics: how each GDB behaved over a window
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS window_gdb_metrics (
    windowid          INTEGER NOT NULL REFERENCES restoration_event_window(windowid) ON DELETE CASCADE,
    gdbid             INTEGER NOT NULL REFERENCES gdb_instance(gdbid) ON DELETE CASCADE,
    vt_start          REAL NOT NULL,
    vt_end            REAL NOT NULL,
    k_start           REAL NOT NULL,
    k_end             REAL NOT NULL,
    e_start           REAL NOT NULL,
    e_end             REAL NOT NULL,
    r_start           REAL NOT NULL,
    r_end             REAL NOT NULL,
    eco_wealth_start  REAL NOT NULL,
    eco_wealth_end    REAL NOT NULL,
    energy_kwh        REAL NOT NULL,
    carbon_kg         REAL NOT NULL,
    roh_integral      REAL NOT NULL,   -- time-integrated RoH exposure
    lane_effective    TEXT NOT NULL CHECK (lane_effective IN ('RESEARCH','EXPPROD','PROD')),
    migration_bytes   INTEGER NOT NULL,
    migration_iops    INTEGER NOT NULL,
    violation_corridor INTEGER NOT NULL CHECK (violation_corridor IN (0,1)),
    violation_roh     INTEGER NOT NULL CHECK (violation_roh IN (0,1)),
    PRIMARY KEY (windowid, gdbid)
);

CREATE INDEX IF NOT EXISTS idx_window_gdb_region_eco
    ON window_gdb_metrics (gdbid, eco_wealth_end DESC, carbon_kg);

----------------------------------------------------------------------
-- 6. Views for agents: shard, GDB, and window-safe candidates
----------------------------------------------------------------------

-- Per-shard blast radius summary on core Lyapunov channels.
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
    b.impactplane,
    SUM(b.impactscore)   AS impactscore_sum,
    MAX(b.vtsensitivity) AS vtsensitivity_max
FROM shardinstance AS s
LEFT JOIN blastradiuslink AS b
  ON b.sourcetype = 'SHARD'
 AND b.sourceid   = CAST(s.shardid AS TEXT)
GROUP BY
    s.shardid,
    s.nodeid,
    s.region,
    s.lane,
    s.kmetric,
    s.emetric,
    s.rmetric,
    s.vtmax,
    b.impactplane;

-- Lane-safe, carbon-negative workload instances that improved residual.
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
    w.gdbid,
    w.ereq_j,
    w.esurplus_j,
    w.rcarbon,
    w.rbiodiv,
    w.vt_before,
    w.vt_after,
    w.decision,
    w.timestamputc
FROM shardinstance AS s
JOIN workloadledger AS w
  ON w.shardid = s.shardid
WHERE
    s.kerdeployable = 1
    AND s.lane IN ('RESEARCH','EXPPROD','PROD')
    AND w.decision = 'ACCEPT'
    AND w.rcarbon IS NOT NULL
    AND w.rcarbon <= 0.13
    AND w.vt_after <= w.vt_before;

-- GDB-level eco_wealth progression over restoration windows.
CREATE VIEW IF NOT EXISTS v_gdb_restoration_progress AS
SELECT
    g.gdbid,
    g.logicalname,
    g.region,
    g.lane,
    g.sizeclass,
    g.tierplacement,
    w.windowid,
    ew.label AS window_label,
    ew.tstart_utc,
    ew.tend_utc,
    w.vt_start,
    w.vt_end,
    w.eco_wealth_start,
    w.eco_wealth_end,
    w.energy_kwh,
    w.carbon_kg,
    w.roh_integral,
    w.violation_corridor,
    w.violation_roh
FROM gdb_instance AS g
JOIN window_gdb_metrics AS w
  ON w.gdbid = g.gdbid
JOIN restoration_event_window AS ew
  ON ew.windowid = w.windowid;

-- Window-safe, eco-restorative GDB instances (always-improve candidates).
CREATE VIEW IF NOT EXISTS v_window_safe_restorative_gdb AS
SELECT
    g.gdbid,
    g.logicalname,
    g.region,
    g.lane,
    g.sizeclass,
    g.tierplacement,
    g.mediumid,
    g.storagepath,
    g.eco_wealth_score,
    m.windowid,
    m.vt_start,
    m.vt_end,
    m.eco_wealth_start,
    m.eco_wealth_end,
    m.carbon_kg,
    m.roh_integral
FROM gdb_instance AS g
JOIN window_gdb_metrics AS m
  ON m.gdbid = g.gdbid
JOIN restoration_event_window AS ew
  ON ew.windowid = m.windowid
WHERE
    ew.lanescope IN ('RESEARCH','EXPPROD','ALL')
    AND m.violation_corridor = 0
    AND m.violation_roh = 0
    AND m.vt_end <= m.vt_start
    AND m.eco_wealth_end >= m.eco_wealth_start;
