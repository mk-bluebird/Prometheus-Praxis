-- filename: db_cyboquatic_gdb_event_windows.sql
-- destination: eco_restoration_shard/db/db_cyboquatic_gdb_event_windows.sql

PRAGMA foreign_keys = ON;

-----------------------------------------------------------------------
-- 1. Mediums, tiers, and GDB physical placement
--    (anchors MediumId, tier, and vault properties)
-----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS storage_medium (
    medium_id          TEXT PRIMARY KEY,          -- e.g. "PHX-DC-01-HDD-A"
    description        TEXT NOT NULL,
    region             TEXT NOT NULL,             -- e.g. "Phoenix-AZ"
    facility_name      TEXT NOT NULL,             -- e.g. "PHX-Edge-1"
    pue                REAL NOT NULL,             -- Power Usage Effectiveness, > 0
    carbon_kg_per_kwh  REAL NOT NULL,             -- grid carbon intensity
    renewable_fraction REAL NOT NULL,             -- 0..1
    roh_floor          REAL NOT NULL,             -- minimum acceptable RoH for workloads on this medium
    latency_band       TEXT NOT NULL,             -- e.g. "TIER0-ULTRA", "TIER1-LOW"
    availability_sla   REAL NOT NULL,             -- 0..1
    created_utc        TEXT NOT NULL,
    updated_utc        TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS storage_tier (
    tier_id       TEXT PRIMARY KEY,              -- e.g. "T0", "T1", "T2", "T3"
    tier_name     TEXT NOT NULL,                 -- e.g. "Hot", "Warm", "Cool", "Cold"
    description   TEXT NOT NULL,
    default_pue   REAL NOT NULL,
    created_utc   TEXT NOT NULL,
    updated_utc   TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS storage_medium_tier (
    medium_id     TEXT NOT NULL REFERENCES storage_medium(medium_id) ON DELETE CASCADE,
    tier_id       TEXT NOT NULL REFERENCES storage_tier(tier_id) ON DELETE CASCADE,
    active        INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    created_utc   TEXT NOT NULL,
    updated_utc   TEXT NOT NULL,
    PRIMARY KEY (medium_id, tier_id)
);

-----------------------------------------------------------------------
-- 2. GDB instances (geospatial-governance knowledge bundles)
--    Each row is a single GDB instance, wired to EcoNet spine ids.
-----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS gdb_instance (
    gdb_id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    logical_name           TEXT NOT NULL,               -- e.g. "PHX-GILA-REACH-001"
    description            TEXT NOT NULL,
    region                 TEXT NOT NULL,               -- matches shardinstance.region
    lane                   TEXT NOT NULL CHECK (lane IN ('RESEARCH','EXPPROD','PROD')),
    size_class             TEXT NOT NULL CHECK (size_class IN ('S','M','L','XL')),
    tier_id                TEXT NOT NULL REFERENCES storage_tier(tier_id),
    medium_id              TEXT NOT NULL REFERENCES storage_medium(medium_id),
    storage_path           TEXT NOT NULL,               -- datalake.storage_path-style
    p_idle_w_per_tb        REAL NOT NULL,
    p_active_w_per_tb      REAL NOT NULL,
    carbon_kg_per_kwh      REAL NOT NULL,
    renewable_fraction     REAL NOT NULL,
    roh_floor              REAL NOT NULL,
    neurorights_compliant  INTEGER NOT NULL CHECK (neurorights_compliant IN (0,1)),
    breach_history_score   REAL NOT NULL,               -- normalized 0..1
    latency_band           TEXT NOT NULL,
    availability_sla       REAL NOT NULL,               -- 0..1
    vt_residual_est        REAL NOT NULL,               -- representative V_t in 0..1 for the instance
    kmetric                REAL NOT NULL,               -- aggregate knowledge metric for GDB
    emetric                REAL NOT NULL,               -- aggregate eco-impact metric
    rmetric                REAL NOT NULL,               -- aggregate risk-of-harm metric
    eco_wealth_score       REAL NOT NULL,               -- instance-level eco_wealth
    roh_ceiling            REAL NOT NULL,               -- hard limit, used for gating
    created_utc            TEXT NOT NULL,
    updated_utc            TEXT NOT NULL,
    UNIQUE (logical_name, region)
);

CREATE INDEX IF NOT EXISTS idx_gdb_region_lane
    ON gdb_instance (region, lane);

CREATE INDEX IF NOT EXISTS idx_gdb_medium_tier
    ON gdb_instance (medium_id, tier_id);

CREATE INDEX IF NOT EXISTS idx_gdb_eco_wealth
    ON gdb_instance (eco_wealth_score DESC);

-----------------------------------------------------------------------
-- 3. GDB binding to EcoNet spine (Lyapunov corridors, shard instances)
--    These tables assume existing EcoNet spine tables:
--      corridordefinition(corridorid, region, lyapchannel, ...)
--      shardinstance(shardid, nodeid, region, lane, kmetric, emetric, rmetric, vtmax, ...)
--      knowledgeecoscore(id, scopetype, scoperefid, kfactor, efactor, rfactor, ...)
-----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS gdb_corridor_binding (
    gdb_id        INTEGER NOT NULL REFERENCES gdb_instance(gdb_id) ON DELETE CASCADE,
    corridor_id   INTEGER NOT NULL,        -- FK to corridordefinition.corridorid (in EcoNet spine DB)
    lyap_channel  TEXT NOT NULL,           -- e.g. "HYDRAULICS", "CARBON", "BIODIVERSITY"
    relevance     REAL NOT NULL,           -- 0..1 importance weight for this corridor in this GDB
    non_offsettable INTEGER NOT NULL CHECK (non_offsettable IN (0,1)),
    created_utc   TEXT NOT NULL,
    updated_utc   TEXT NOT NULL,
    PRIMARY KEY (gdb_id, corridor_id)
);

CREATE INDEX IF NOT EXISTS idx_gdb_corridor_channel
    ON gdb_corridor_binding (lyap_channel, non_offsettable);

CREATE TABLE IF NOT EXISTS gdb_shard_binding (
    gdb_id          INTEGER NOT NULL REFERENCES gdb_instance(gdb_id) ON DELETE CASCADE,
    shard_id        INTEGER NOT NULL,    -- FK to shardinstance.shardid in EcoNet spine
    impact_role     TEXT NOT NULL CHECK (impact_role IN ('PRIMARY','SUPPORTING','DIAGNOSTIC')),
    kmetric         REAL NOT NULL,
    emetric         REAL NOT NULL,
    rmetric         REAL NOT NULL,
    vt_contrib      REAL NOT NULL,       -- shard’s contribution to GDB residual
    eco_wealth_contrib REAL NOT NULL,    -- shard’s contribution to GDB eco_wealth
    created_utc     TEXT NOT NULL,
    updated_utc     TEXT NOT NULL,
    PRIMARY KEY (gdb_id, shard_id)
);

CREATE INDEX IF NOT EXISTS idx_gdb_shard_id
    ON gdb_shard_binding (shard_id);

-----------------------------------------------------------------------
-- 4. Blast radius link from schemas/shards into GDB instances
--    This is the spatial lever that ties abstract kernels to specific GDBs.
-----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS blast_radius_link (
    link_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    gdb_id         INTEGER NOT NULL REFERENCES gdb_instance(gdb_id) ON DELETE CASCADE,
    source_type    TEXT NOT NULL CHECK (source_type IN ('ALNSCHEMA','SHARD','WORKLOAD')),
    source_ref     TEXT NOT NULL,        -- e.g. schema name, shardid as text, workload id
    impact_plane   TEXT NOT NULL,        -- "HYDRAULICS", "CARBON", "BIODIVERSITY", "MATERIALS", etc.
    impact_kind    TEXT NOT NULL CHECK (impact_kind IN ('STABILIZING','DESTABILIZING','NEUTRAL')),
    impact_weight  REAL NOT NULL,        -- 0..1 magnitude contribution for this plane
    vt_delta_est   REAL NOT NULL,        -- expected delta in V_t per event-window (signed)
    k_delta_est    REAL NOT NULL,
    e_delta_est    REAL NOT NULL,
    r_delta_est    REAL NOT NULL,
    roh_delta_est  REAL NOT NULL,
    neighborhood_radius_m REAL NOT NULL, -- physical blast radius
    neighborhood_hops    INTEGER NOT NULL,
    created_utc    TEXT NOT NULL,
    updated_utc    TEXT NOT NULL,
    UNIQUE (gdb_id, source_type, source_ref, impact_plane)
);

CREATE INDEX IF NOT EXISTS idx_blast_gdb_plane
    ON blast_radius_link (gdb_id, impact_plane);

CREATE INDEX IF NOT EXISTS idx_blast_source
    ON blast_radius_link (source_type, source_ref);

-----------------------------------------------------------------------
-- 5. Ecological-restoration event windows (bounded evaluation periods)
-----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS restoration_event_window (
    window_id              INTEGER PRIMARY KEY AUTOINCREMENT,
    region                 TEXT NOT NULL,          -- e.g. "Phoenix-AZ"
    corridor_region_label  TEXT NOT NULL,          -- e.g. "Phoenix-AZ – Gila reach"
    t_start_utc            TEXT NOT NULL,
    t_end_utc              TEXT NOT NULL,
    lane_scope             TEXT NOT NULL CHECK (lane_scope IN ('RESEARCH','EXPPROD','PROD','ALL')),
    migration_budget_bytes INTEGER NOT NULL,       -- max allowed data movement within window
    migration_budget_iops  INTEGER NOT NULL,       -- max I/O ops for migrations
    carbon_budget_kg       REAL NOT NULL,         -- optional carbon budget for this window
    roh_budget             REAL NOT NULL,         -- max allowable aggregate RoH increment
    created_utc            TEXT NOT NULL,
    updated_utc            TEXT NOT NULL,
    UNIQUE (region, t_start_utc, t_end_utc)
);

CREATE INDEX IF NOT EXISTS idx_window_region_time
    ON restoration_event_window (region, t_start_utc, t_end_utc);

-----------------------------------------------------------------------
-- 6. Window–GDB aggregates (V_t, K/E/R trajectories, eco_wealth)
--    Filled by telemetry and analysis jobs once per window.
-----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS window_gdb_metrics (
    window_id          INTEGER NOT NULL REFERENCES restoration_event_window(window_id) ON DELETE CASCADE,
    gdb_id             INTEGER NOT NULL REFERENCES gdb_instance(gdb_id) ON DELETE CASCADE,
    vt_start           REAL NOT NULL,
    vt_end             REAL NOT NULL,
    k_start            REAL NOT NULL,
    k_end              REAL NOT NULL,
    e_start            REAL NOT NULL,
    e_end              REAL NOT NULL,
    r_start            REAL NOT NULL,
    r_end              REAL NOT NULL,
    eco_wealth_start   REAL NOT NULL,
    eco_wealth_end     REAL NOT NULL,
    energy_kwh         REAL NOT NULL,
    carbon_kg          REAL NOT NULL,
    renewable_fraction REAL NOT NULL,
    roh_integral       REAL NOT NULL,       -- time-integrated RoH exposure
    lane_effective     TEXT NOT NULL CHECK (lane_effective IN ('RESEARCH','EXPPROD','PROD')),
    migration_bytes    INTEGER NOT NULL,
    migration_iops     INTEGER NOT NULL,
    violation_corridor INTEGER NOT NULL CHECK (violation_corridor IN (0,1)),
    violation_roh      INTEGER NOT NULL CHECK (violation_roh IN (0,1)),
    violation_neurorights INTEGER NOT NULL CHECK (violation_neurorights IN (0,1)),
    created_utc        TEXT NOT NULL,
    updated_utc        TEXT NOT NULL,
    PRIMARY KEY (window_id, gdb_id)
);

CREATE INDEX IF NOT EXISTS idx_window_gdb_vt_delta
    ON window_gdb_metrics (window_id, vt_end - vt_start);

-----------------------------------------------------------------------
-- 7. Candidate migration plans and decisions per window
--    Non-actuating plans + governed approvals.
-----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS migration_plan (
    plan_id              INTEGER PRIMARY KEY AUTOINCREMENT,
    window_id            INTEGER NOT NULL REFERENCES restoration_event_window(window_id) ON DELETE CASCADE,
    gdb_id               INTEGER NOT NULL REFERENCES gdb_instance(gdb_id) ON DELETE CASCADE,
    from_medium_id       TEXT NOT NULL,
    from_tier_id         TEXT NOT NULL,
    to_medium_id         TEXT NOT NULL,
    to_tier_id           TEXT NOT NULL,
    estimated_bytes      INTEGER NOT NULL,
    estimated_iops       INTEGER NOT NULL,
    estimated_energy_kwh REAL NOT NULL,
    estimated_carbon_kg  REAL NOT NULL,
    tag_compat_ok        INTEGER NOT NULL CHECK (tag_compat_ok IN (0,1)),
    roh_ok               INTEGER NOT NULL CHECK (roh_ok IN (0,1)),
    neurorights_ok       INTEGER NOT NULL CHECK (neurorights_ok IN (0,1)),
    lane_scope           TEXT NOT NULL CHECK (lane_scope IN ('RESEARCH','EXPPROD','PROD')),
    vt_delta_est         REAL NOT NULL,     -- expected delta V_t if applied
    k_delta_est          REAL NOT NULL,
    e_delta_est          REAL NOT NULL,
    r_delta_est          REAL NOT NULL,
    eco_wealth_delta_est REAL NOT NULL,
    score_local          REAL NOT NULL,     -- inner-loop score
    score_portfolio      REAL NOT NULL,     -- outer-loop eco-placement score
    status               TEXT NOT NULL CHECK (status IN ('CANDIDATE','RECOMMENDED','APPROVED','REJECTED','APPLIED')),
    author_agent_id      TEXT NOT NULL,     -- agent or process that proposed the plan
    created_utc          TEXT NOT NULL,
    updated_utc          TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_migration_window_status
    ON migration_plan (window_id, status);

CREATE INDEX IF NOT EXISTS idx_migration_gdb
    ON migration_plan (gdb_id, status);

-----------------------------------------------------------------------
-- 8. Neurorights and RoH governance envelopes per SKO/GDB
--    These are the hard gates checked by outer-loop governance.
-----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS neurorights_envelope (
    envelope_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    sko_id               TEXT NOT NULL,            -- SKO identifier in EcoNet spine
    gdb_id               INTEGER NOT NULL REFERENCES gdb_instance(gdb_id) ON DELETE CASCADE,
    neurorights_sensitive INTEGER NOT NULL CHECK (neurorights_sensitive IN (0,1)),
    roh_max              REAL NOT NULL,            -- RoH ceiling R_max
    roh_cumulative       REAL NOT NULL,            -- accumulated RoH exposure
    roh_margin           REAL NOT NULL,            -- roh_max - roh_cumulative
    neurorights_vault_tag TEXT NOT NULL,           -- required vault subset tag
    last_window_id       INTEGER,                  -- last evaluated window
    created_utc          TEXT NOT NULL,
    updated_utc          TEXT NOT NULL,
    UNIQUE (sko_id, gdb_id)
);

CREATE INDEX IF NOT EXISTS idx_neurorights_sko
    ON neurorights_envelope (sko_id);

CREATE INDEX IF NOT EXISTS idx_neurorights_gdb
    ON neurorights_envelope (gdb_id);

-----------------------------------------------------------------------
-- 9. Views for agent-facing analytics
--    v_gdb_blast_radius: join GDB, blast radius, and KER
--    v_window_safe_carbon_negative: window/GDB pairs that improved V_t
--                                   and are carbon-negative and RoH-safe
-----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_gdb_blast_radius AS
SELECT
    g.gdb_id,
    g.logical_name,
    g.region,
    g.lane,
    g.kmetric,
    g.emetric,
    g.rmetric,
    g.vt_residual_est AS vt_gdb,
    g.eco_wealth_score,
    b.source_type,
    b.source_ref,
    b.impact_plane,
    b.impact_kind,
    b.impact_weight,
    b.vt_delta_est,
    b.k_delta_est,
    b.e_delta_est,
    b.r_delta_est,
    b.roh_delta_est,
    b.neighborhood_radius_m,
    b.neighborhood_hops
FROM gdb_instance g
JOIN blast_radius_link b
  ON b.gdb_id = g.gdb_id;

CREATE VIEW IF NOT EXISTS v_window_safe_carbon_negative AS
SELECT
    w.window_id,
    w.region,
    w.corridor_region_label,
    g.gdb_id,
    g.logical_name,
    g.lane,
    m.vt_start,
    m.vt_end,
    (m.vt_end - m.vt_start) AS vt_delta,
    m.k_start,
    m.k_end,
    m.e_start,
    m.e_end,
    m.r_start,
    m.r_end,
    m.eco_wealth_start,
    m.eco_wealth_end,
    m.energy_kwh,
    m.carbon_kg,
    CASE WHEN m.energy_kwh > 0 THEN (m.eco_wealth_end - m.eco_wealth_start) / m.energy_kwh
         ELSE NULL END AS eco_wealth_per_kwh,
    m.renewable_fraction,
    m.roh_integral,
    m.lane_effective,
    m.migration_bytes,
    m.migration_iops,
    m.violation_corridor,
    m.violation_roh,
    m.violation_neurorights
FROM restoration_event_window w
JOIN window_gdb_metrics m
  ON m.window_id = w.window_id
JOIN gdb_instance g
  ON g.gdb_id = m.gdb_id
WHERE
    m.vt_end <= m.vt_start
    AND m.carbon_kg <= 0.0
    AND m.violation_corridor = 0
    AND m.violation_roh = 0
    AND m.violation_neurorights = 0;

-----------------------------------------------------------------------
-- 10. Helper table for eco-placement objective J_{i,m,d}
--     This is a ledger of placement evaluations used in outer loop.
-----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_placement_objective (
    objective_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    window_id        INTEGER NOT NULL REFERENCES restoration_event_window(window_id) ON DELETE CASCADE,
    gdb_id           INTEGER NOT NULL REFERENCES gdb_instance(gdb_id) ON DELETE CASCADE,
    sko_id           TEXT NOT NULL,          -- SKO identifier
    medium_id        TEXT NOT NULL,
    tier_id          TEXT NOT NULL,
    w_e              REAL NOT NULL,          -- weight on eco-impact E
    w_c              REAL NOT NULL,          -- weight on carbon risk C
    w_l              REAL NOT NULL,          -- weight on latency L
    w_r              REAL NOT NULL,          -- weight on risk R
    eco_wealth_i     REAL NOT NULL,          -- EW_i for this SKO
    gamma_ew         REAL NOT NULL,          -- gamma(EW_i)
    e_ij             REAL NOT NULL,          -- E_{i,j}
    c_ij             REAL NOT NULL,          -- C_{i,j} (normalized carbon)
    l_ij             REAL NOT NULL,          -- L_{i,j} (latency)
    r_ij             REAL NOT NULL,          -- R_{i,j}
    j_value          REAL NOT NULL,          -- J_{i,m,d}
    created_utc      TEXT NOT NULL,
    updated_utc      TEXT NOT NULL,
    UNIQUE (window_id, gdb_id, sko_id, medium_id, tier_id)
);

CREATE INDEX IF NOT EXISTS idx_eco_placement_window
    ON eco_placement_objective (window_id, j_value);
