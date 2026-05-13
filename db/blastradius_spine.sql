-- filename: db/blastradius_spine.sql
-- destination: EcoNet/sqlite/db/blastradius_spine.sql
-- purpose:
--   Central EcoNet SQLite spine for cyboquatic blast-radius, energy/carbon/restoration
--   scoring, and always-improve routing over shards, reusing existing ecosafety grammar.
-- non-actuating, read/write on metrics only, no actuator handles.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Core blast radius and adjacency objects
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS blastradius_object (
    radius_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    -- foreign key into existing shardinstance table (from eco-index)
    shard_id         INTEGER NOT NULL,
    -- physical and temporal reach of disturbance from this shard
    radius_meters    REAL    NOT NULL,
    radius_hours     REAL    NOT NULL,
    hops             INTEGER NOT NULL,
    -- qualitative propagation type (hydraulic, bio, energy, social, mixed)
    propagation_type TEXT    NOT NULL,
    -- corridor band tags for this radius object (gold/safe/hard)
    corridor_band    TEXT    NOT NULL,
    continuity_grade TEXT    NOT NULL,  -- e.g. MT6883-A, MT6883-B
    -- provenance anchor (Phoenix/Teslaswarm hex)
    tbr2026v1_hex    TEXT    NOT NULL,
    -- KER summary at time of radius evaluation
    k_at_eval        REAL    NOT NULL,
    e_at_eval        REAL    NOT NULL,
    r_at_eval        REAL    NOT NULL,
    vt_at_eval       REAL    NOT NULL,
    UNIQUE (shard_id, propagation_type),
    FOREIGN KEY (shard_id) REFERENCES shardinstance(shardid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_blastradius_shard
    ON blastradius_object (shard_id);

CREATE INDEX IF NOT EXISTS idx_blastradius_band
    ON blastradius_object (corridor_band, continuity_grade);

----------------------------------------------------------------------
-- 2. Adjacency graph between cyboquatic nodes (canal-like topology)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS adjacency_graph (
    edge_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    from_shard_id   INTEGER NOT NULL,
    to_shard_id     INTEGER NOT NULL,
    -- physical spacing between centroids
    distance_meters REAL    NOT NULL,
    -- typical travel time at nominal conditions
    travel_hours    REAL    NOT NULL,
    -- connectivity type (canal, force_main, open_channel, vault, air_plenum)
    edge_type       TEXT    NOT NULL,
    -- directionality flags
    bidirectional   INTEGER NOT NULL CHECK (bidirectional IN (0,1)),
    -- normalized canal coordinate (0..1) for this edge, precomputed or updated by jobs
    r_canal_edge    REAL    NOT NULL,
    UNIQUE (from_shard_id, to_shard_id, edge_type),
    FOREIGN KEY (from_shard_id) REFERENCES shardinstance(shardid) ON DELETE CASCADE,
    FOREIGN KEY (to_shard_id)   REFERENCES shardinstance(shardid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_adj_from_to
    ON adjacency_graph (from_shard_id, to_shard_id);

CREATE INDEX IF NOT EXISTS idx_adj_canal
    ON adjacency_graph (r_canal_edge);

----------------------------------------------------------------------
-- 3. Energy, carbon, and eco-restoration metrics per shard-window
--    (non-actuating: these are diagnostics, computed by external jobs)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS shard_ecoscore (
    ecoscore_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    shard_id           INTEGER NOT NULL,
    -- energy metrics over this window
    energy_in_kwh      REAL    NOT NULL,
    energy_out_kwh     REAL    NOT NULL,
    renewable_frac     REAL    NOT NULL,  -- 0..1 portion of energy_out that is renewable
    -- carbon metrics (positive = emitted, negative = net removed)
    co2_emitted_kg     REAL    NOT NULL,
    co2_removed_kg     REAL    NOT NULL,
    -- restoration metrics (e.g. recharged water, pollutants removed)
    water_recharged_m3 REAL    NOT NULL,
    pollutants_removed_kg REAL  NOT NULL,
    biodiversity_index REAL    NOT NULL,  -- 0..1 normalized, from shard planes
    -- derived scalar scores, normalized 0..1
    energy_eff_score   REAL    NOT NULL,
    carbon_score       REAL    NOT NULL,
    restoration_score  REAL    NOT NULL,
    -- window metadata
    t_start_utc        TEXT    NOT NULL,
    t_end_utc          TEXT    NOT NULL,
    lane               TEXT    NOT NULL,  -- RESEARCH, EXPPROD, PROD
    -- KER snapshot for this ecoscore computation
    k_factor           REAL    NOT NULL,
    e_factor           REAL    NOT NULL,
    r_factor           REAL    NOT NULL,
    -- provenance and signer
    evidence_hex       TEXT    NOT NULL,
    signing_did        TEXT    NOT NULL,
    FOREIGN KEY (shard_id) REFERENCES shardinstance(shardid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_shard_ecoscore_shard_time
    ON shard_ecoscore (shard_id, t_start_utc, t_end_utc);

CREATE INDEX IF NOT EXISTS idx_shard_ecoscore_lane
    ON shard_ecoscore (lane, energy_eff_score, carbon_score, restoration_score);

----------------------------------------------------------------------
-- 4. Always-improve corridor definitions for energy/carbon/restoration
--    (pure metadata; controllers stay in Rust/ALN)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_always_improve_policy (
    policy_id                INTEGER PRIMARY KEY AUTOINCREMENT,
    name                     TEXT    NOT NULL UNIQUE,
    description              TEXT    NOT NULL,
    -- minimum improvement over previous window (fractional, can be 0)
    min_delta_energy_eff     REAL    NOT NULL,
    min_delta_carbon_score   REAL    NOT NULL,
    min_delta_restoration    REAL    NOT NULL,
    -- minimum absolute thresholds for eligibility
    min_energy_eff_score     REAL    NOT NULL,
    min_carbon_score         REAL    NOT NULL,
    min_restoration_score    REAL    NOT NULL,
    -- lane this policy applies to
    lane                     TEXT    NOT NULL, -- RESEARCH, EXPPROD, PROD
    -- link into ecosafety corridordefinition if desired
    corridor_var_energy      TEXT    NOT NULL,
    corridor_var_carbon      TEXT    NOT NULL,
    corridor_var_restoration TEXT    NOT NULL,
    active                   INTEGER NOT NULL CHECK (active IN (0,1))
);

----------------------------------------------------------------------
-- 5. Views: shard + blast-radius + canal + ecoscore
----------------------------------------------------------------------

-- 5.1 Canonical shard residual + KER view is assumed as v_shard_ker
--      (as defined in your ecosafety-corridors slice), here we only consume it.

-- Per-shard blast-radius summary
CREATE VIEW IF NOT EXISTS v_shard_blast AS
SELECT
    s.shardid              AS shard_id,
    s.nodeid               AS node_id,
    s.region               AS region,
    s.medium               AS medium,
    s.lane                 AS lane,
    s.kmetric              AS k_shard,
    s.emetric              AS e_shard,
    s.rmetric              AS r_shard,
    s.vtmax                AS vt_shard,
    b.radius_meters,
    b.radius_hours,
    b.hops,
    b.propagation_type,
    b.corridor_band,
    b.continuity_grade,
    b.tbr2026v1_hex,
    b.k_at_eval,
    b.e_at_eval,
    b.r_at_eval,
    b.vt_at_eval
FROM shardinstance AS s
JOIN blastradius_object AS b
  ON b.shard_id = s.shardid;

-- Per-shard canal-normalized velocity coordinate (rcanal) derived from adjacency
CREATE VIEW IF NOT EXISTS v_shard_canal AS
SELECT
    s.shardid       AS shard_id,
    s.nodeid        AS node_id,
    s.region        AS region,
    s.lane          AS lane,
    -- aggregate edge-level r_canal_edge into a shard coordinate (max is conservative)
    MAX(a.r_canal_edge) AS r_canal,
    COUNT(a.edge_id)    AS edge_count
FROM shardinstance AS s
LEFT JOIN adjacency_graph AS a
  ON a.from_shard_id = s.shardid
GROUP BY
    s.shardid,
    s.nodeid,
    s.region,
    s.lane;

-- Combined ecoscore + blast + canal for routing and siting agents
CREATE VIEW IF NOT EXISTS v_shard_eco_blast AS
SELECT
    e.shard_id,
    s.nodeid,
    s.region,
    s.medium,
    e.lane,
    -- ecoscore scalars
    e.energy_eff_score,
    e.carbon_score,
    e.restoration_score,
    e.k_factor,
    e.e_factor,
    e.r_factor,
    e.t_start_utc,
    e.t_end_utc,
    -- blast radius and canal coordinates if present
    b.radius_meters,
    b.radius_hours,
    b.hops,
    b.propagation_type,
    b.corridor_band AS blast_band,
    c.r_canal       AS r_canal,
    -- shard-level KER
    s.kmetric       AS k_shard,
    s.emetric       AS e_shard,
    s.rmetric       AS r_shard,
    s.vtmax         AS vt_shard
FROM shard_ecoscore AS e
JOIN shardinstance AS s
  ON s.shardid = e.shard_id
LEFT JOIN v_shard_blast AS b
  ON b.shard_id = e.shard_id
LEFT JOIN v_shard_canal AS c
  ON c.shard_id = e.shard_id;

----------------------------------------------------------------------
-- 6. Always-improve eligibility view (non-actuating, advisory only)
----------------------------------------------------------------------

-- Assumes ecoscores are written in chronological order per shard.
-- We compare each window to the immediately preceding window.
CREATE VIEW IF NOT EXISTS v_shard_eco_improvement AS
WITH ordered AS (
    SELECT
        ecoscore_id,
        shard_id,
        lane,
        energy_eff_score,
        carbon_score,
        restoration_score,
        t_start_utc,
        t_end_utc,
        LAG(energy_eff_score) OVER (
            PARTITION BY shard_id, lane ORDER BY t_start_utc
        ) AS prev_energy_eff_score,
        LAG(carbon_score) OVER (
            PARTITION BY shard_id, lane ORDER BY t_start_utc
        ) AS prev_carbon_score,
        LAG(restoration_score) OVER (
            PARTITION BY shard_id, lane ORDER BY t_start_utc
        ) AS prev_restoration_score
    FROM shard_ecoscore
),
deltas AS (
    SELECT
        ecoscore_id,
        shard_id,
        lane,
        energy_eff_score,
        carbon_score,
        restoration_score,
        t_start_utc,
        t_end_utc,
        prev_energy_eff_score,
        prev_carbon_score,
        prev_restoration_score,
        CASE
            WHEN prev_energy_eff_score IS NULL THEN NULL
            ELSE energy_eff_score - prev_energy_eff_score
        END AS delta_energy_eff,
        CASE
            WHEN prev_carbon_score IS NULL THEN NULL
            ELSE carbon_score - prev_carbon_score
        END AS delta_carbon_score,
        CASE
            WHEN prev_restoration_score IS NULL THEN NULL
            ELSE restoration_score - prev_restoration_score
        END AS delta_restoration_score
    FROM ordered
)
SELECT
    d.ecoscore_id,
    d.shard_id,
    d.lane,
    d.energy_eff_score,
    d.carbon_score,
    d.restoration_score,
    d.delta_energy_eff,
    d.delta_carbon_score,
    d.delta_restoration_score,
    d.t_start_utc,
    d.t_end_utc,
    p.name AS policy_name,
    p.min_delta_energy_eff,
    p.min_delta_carbon_score,
    p.min_delta_restoration,
    p.min_energy_eff_score,
    p.min_carbon_score,
    p.min_restoration_score,
    CASE
        WHEN p.active = 1
         AND d.energy_eff_score    >= p.min_energy_eff_score
         AND d.carbon_score        >= p.min_carbon_score
         AND d.restoration_score   >= p.min_restoration_score
         AND (d.delta_energy_eff   IS NULL OR d.delta_energy_eff   >= p.min_delta_energy_eff)
         AND (d.delta_carbon_score IS NULL OR d.delta_carbon_score >= p.min_delta_carbon_score)
         AND (d.delta_restoration_score IS NULL OR d.delta_restoration_score >= p.min_delta_restoration)
        THEN 1
        ELSE 0
    END AS always_improve_ok
FROM deltas AS d
LEFT JOIN eco_always_improve_policy AS p
  ON p.lane = d.lane
WHERE p.active = 1;
