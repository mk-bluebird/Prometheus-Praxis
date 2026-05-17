-- filename: dbecoreward_governance_eco_karma.sql
-- destination: eco_restoration_shard/sql/governance/dbecoreward_governance_eco_karma.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Plane / residual governance: capability floors, time-decay, r_trust
-------------------------------------------------------------------------------

-- Master risk planes (energy, carbon, biodiversity, topology, etc.) and capability floors.
CREATE TABLE IF NOT EXISTS risk_plane (
    plane_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    plane_name      TEXT NOT NULL UNIQUE,               -- e.g. 'energy', 'carbon', 'biodiversity', 'topology', 'healthcare_biomass'
    -- Non-offsettable planes cannot be compensated by other planes when computing residual.
    non_offsettable INTEGER NOT NULL DEFAULT 0 CHECK (non_offsettable IN (0,1)),
    -- Capability floor: minimum acceptable capability (0..1) for this plane.
    capability_floor REAL NOT NULL DEFAULT 0.0,
    -- Hard lower bound for the floor (prevents dilution over time).
    capability_floor_hard_min REAL NOT NULL DEFAULT 0.0,
    -- Optional description.
    description     TEXT
);

CREATE INDEX IF NOT EXISTS idx_risk_plane_nonoffsettable
    ON risk_plane(non_offsettable);


-- Global KER bands with capability floor versioning.
CREATE TABLE IF NOT EXISTS ker_band_version (
    ker_band_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    band_name       TEXT NOT NULL,                      -- e.g. 'RESEARCH', 'EXPPROD', 'PROD'
    version_epoch   INTEGER NOT NULL,                   -- EvolutionEpoch integer id
    -- Capability floor snapshot per band (0..1).
    k_floor         REAL NOT NULL,
    e_floor         REAL NOT NULL,
    r_ceiling       REAL NOT NULL,
    -- Prevent dilution: floors are monotone non-decreasing per band.
    CHECK (k_floor >= 0.0 AND k_floor <= 1.0),
    CHECK (e_floor >= 0.0 AND e_floor <= 1.0),
    CHECK (r_ceiling >= 0.0 AND r_ceiling <= 1.0),
    UNIQUE (band_name, version_epoch)
);

CREATE INDEX IF NOT EXISTS idx_ker_band_version_band_epoch
    ON ker_band_version(band_name, version_epoch);


-- Lyapunov residual snapshots including trust plane r_trust and topology risk.
CREATE TABLE IF NOT EXISTS ker_residual_snapshot (
    snapshot_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    -- Foreign key to workload / kernel identity would live in the existing spine,
    -- here we model just the residual vector.
    region          TEXT NOT NULL,                      -- e.g. 'Phoenix-AZ'
    kernel_id       TEXT NOT NULL,                      -- logical kernel identity
    epoch           INTEGER NOT NULL,                   -- EvolutionEpoch at time of snapshot
    -- Aggregate KER for the window.
    k_mean          REAL NOT NULL,
    e_mean          REAL NOT NULL,
    r_mean          REAL NOT NULL,
    -- Plane-specific risk coordinates (0..1).
    r_energy        REAL NOT NULL,
    r_carbon        REAL NOT NULL,
    r_biodiversity  REAL NOT NULL,
    r_topology      REAL NOT NULL,
    r_trust         REAL NOT NULL,                      -- trust-plane coordinate (0..1)
    -- Time-decay effective residual r_eff with monotone non-increase constraint enforced in Rust.
    r_eff           REAL NOT NULL,
    window_start_utc TEXT NOT NULL,
    window_end_utc   TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_ker_residual_kernel_epoch
    ON ker_residual_snapshot(kernel_id, epoch, region);


-------------------------------------------------------------------------------
-- 2. EvolutionEpoch and fork-binding
-------------------------------------------------------------------------------

-- Evolution epochs bind reward policies, KER bands, and governance upgrades.
CREATE TABLE IF NOT EXISTS evolution_epoch (
    evolution_epoch_id INTEGER PRIMARY KEY AUTOINCREMENT,
    epoch_label        TEXT NOT NULL UNIQUE,           -- e.g. 'PhoenixWater_2026Q2'
    -- Fork-binding: identifies the policy fork this epoch belongs to.
    fork_id            TEXT NOT NULL,                  -- e.g. git tag or ALN policy id
    parent_epoch_id    INTEGER REFERENCES evolution_epoch(evolution_epoch_id),
    -- Effective from (inclusive).
    effective_from_utc TEXT NOT NULL,
    -- Effective until (exclusive); NULL means open-ended.
    effective_until_utc TEXT,
    -- Notes about what changed (K/E floors, reward split rules, etc.).
    notes              TEXT
);

CREATE INDEX IF NOT EXISTS idx_evolution_epoch_fork
    ON evolution_epoch(fork_id);


-------------------------------------------------------------------------------
-- 3. Boot reward audit and eco-split with responsibility deltas
-------------------------------------------------------------------------------

-- Boot reward events (initial eco-bootstrapping rewards).
CREATE TABLE IF NOT EXISTS boot_reward_event (
    boot_reward_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    evolution_epoch_id INTEGER NOT NULL REFERENCES evolution_epoch(evolution_epoch_id) ON DELETE CASCADE,
    -- Actor / portfolio identifier (e.g. DID, Bostrom address).
    actor_did        TEXT NOT NULL,
    -- Baseline responsibility metrics at boot time (0..1).
    r_baseline       REAL NOT NULL,
    k_baseline       REAL NOT NULL,
    e_baseline       REAL NOT NULL,
    -- Tokens awarded in this boot event.
    tokens_awarded   REAL NOT NULL,
    -- Eco-split fractions (sum <= 1.0) between actors/planes.
    eco_split_json   TEXT NOT NULL,                   -- JSON mapping destination->fraction
    -- Responsibility delta in subsequent audit windows.
    r_delta_target   REAL NOT NULL,                   -- target residual reduction (>= 0)
    -- Whether this boot reward is fully audited and closed.
    audited_closed   INTEGER NOT NULL DEFAULT 0 CHECK (audited_closed IN (0,1)),
    created_utc      TEXT NOT NULL,
    audited_utc      TEXT
);

CREATE INDEX IF NOT EXISTS idx_boot_reward_actor_epoch
    ON boot_reward_event(actor_did, evolution_epoch_id);


-- Responsibility delta audits per boot reward.
CREATE TABLE IF NOT EXISTS boot_reward_responsibility_audit (
    audit_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    boot_reward_id   INTEGER NOT NULL REFERENCES boot_reward_event(boot_reward_id) ON DELETE CASCADE,
    audit_window_start_utc TEXT NOT NULL,
    audit_window_end_utc   TEXT NOT NULL,
    -- Observed responsibility (residual) metrics after boot.
    r_observed       REAL NOT NULL,
    -- Delta relative to baseline (positive means improvement).
    r_delta_observed REAL NOT NULL,
    -- Whether minimum responsibility improvement target was met.
    target_met       INTEGER NOT NULL CHECK (target_met IN (0,1)),
    -- Snapshot of trust-plane coordinate at audit time.
    r_trust_observed REAL NOT NULL,
    created_utc      TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_boot_reward_audit_brid
    ON boot_reward_responsibility_audit(boot_reward_id);


-------------------------------------------------------------------------------
-- 4. Quadratic oracle staking for ECO_REPAIR_KARMA_DRAIN
-------------------------------------------------------------------------------

-- Staking pools for eco repair / karma drain.
CREATE TABLE IF NOT EXISTS eco_repair_stake_pool (
    pool_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    pool_name        TEXT NOT NULL UNIQUE,             -- e.g. 'ECO_REPAIR_KARMA_DRAIN'
    evolution_epoch_id INTEGER NOT NULL REFERENCES evolution_epoch(evolution_epoch_id) ON DELETE CASCADE,
    -- Total stake (linear sum) for informational purposes.
    total_stake      REAL NOT NULL DEFAULT 0.0,
    -- Quadratic-weighted effective stake, recomputed by Rust oracle.
    total_effective_stake REAL NOT NULL DEFAULT 0.0,
    created_utc      TEXT NOT NULL,
    updated_utc      TEXT NOT NULL
);

-- Individual stakes.
CREATE TABLE IF NOT EXISTS eco_repair_stake (
    stake_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    pool_id          INTEGER NOT NULL REFERENCES eco_repair_stake_pool(pool_id) ON DELETE CASCADE,
    actor_did        TEXT NOT NULL,
    -- Raw stake amount.
    stake_amount     REAL NOT NULL,
    -- Quadratic oracle weight (sqrt-based or other monotone function).
    effective_weight REAL NOT NULL,
    -- Karma drain / repair direction: +1 repair, -1 drain.
    direction        INTEGER NOT NULL CHECK (direction IN (-1, 1)),
    created_utc      TEXT NOT NULL
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_eco_repair_stake_actor_pool
    ON eco_repair_stake(pool_id, actor_did);


-------------------------------------------------------------------------------
-- 5. Brain-bound one-time delegations
-------------------------------------------------------------------------------

-- One-time delegations for eco governance and staking (brain-bound, non-replayable).
CREATE TABLE IF NOT EXISTS brain_bound_delegation (
    delegation_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    delegator_did    TEXT NOT NULL,
    delegatee_did    TEXT NOT NULL,
    evolution_epoch_id INTEGER NOT NULL REFERENCES evolution_epoch(evolution_epoch_id) ON DELETE CASCADE,
    -- Scope of delegation, e.g. 'ECO_REPAIR_STAKE', 'LANE_VOTE', 'PORTFOLIO_VOTE'.
    scope            TEXT NOT NULL,
    -- One-time use token / nonce (hash string).
    delegation_token TEXT NOT NULL UNIQUE,
    -- Whether this delegation has been consumed.
    consumed         INTEGER NOT NULL DEFAULT 0 CHECK (consumed IN (0,1)),
    created_utc      TEXT NOT NULL,
    consumed_utc     TEXT
);

CREATE INDEX IF NOT EXISTS idx_brain_bound_delegation_scope
    ON brain_bound_delegation(scope, evolution_epoch_id);


-------------------------------------------------------------------------------
-- 6. Sensor telemetry -> risk coordinates (first-class)
-------------------------------------------------------------------------------

-- Registry mapping sensor streams to risk planes and coordinate transforms.
CREATE TABLE IF NOT EXISTS sensor_risk_mapping (
    sensor_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    sensor_name      TEXT NOT NULL,                    -- e.g. 'PhoenixWater_NO2', 'PollinatorIndex_Tempe'
    plane_name       TEXT NOT NULL,                    -- FK into risk_plane.plane_name (logical)
    -- Transform reference (ALN or code id) that maps raw sensor units to normalized risk coordinate 0..1.
    transform_id     TEXT NOT NULL,                    -- e.g. 'NO2_to_r_energy_v1'
    -- Whether this mapping is active.
    active           INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    UNIQUE (sensor_name, plane_name, transform_id)
);

CREATE INDEX IF NOT EXISTS idx_sensor_risk_plane
    ON sensor_risk_mapping(plane_name, active);


-- Materialized risk coordinates per sensor/time window.
CREATE TABLE IF NOT EXISTS sensor_risk_coordinate (
    coordinate_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    sensor_id        INTEGER NOT NULL REFERENCES sensor_risk_mapping(sensor_id) ON DELETE CASCADE,
    window_start_utc TEXT NOT NULL,
    window_end_utc   TEXT NOT NULL,
    -- Normalized risk coordinate in 0..1 for the mapped plane.
    r_value          REAL NOT NULL,
    created_utc      TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_sensor_risk_coord_sensor_time
    ON sensor_risk_coordinate(sensor_id, window_start_utc);


-------------------------------------------------------------------------------
-- 7. EcoWealthView over statements + KER (including KER residual)
-------------------------------------------------------------------------------

-- StewardEcoWealthStatement core table is assumed to exist; here we create a
-- view that joins it with KER residual snapshots for KER-aware eco-wealth.
-- We keep this as a VIEW so agents can query without duplicating data.

DROP VIEW IF EXISTS ecowealthview;

CREATE VIEW ecowealthview AS
SELECT
    s.statement_id,
    s.portfolio_id,
    s.region,
    s.period_start_utc,
    s.period_end_utc,
    s.biomass_total,
    s.pollinator_index,
    s.eco_tokens_earned,
    k.k_mean,
    k.e_mean,
    k.r_mean,
    k.r_trust,
    k.r_eff,
    k.epoch
FROM
    steward_eco_wealth_statement AS s
JOIN
    ker_residual_snapshot AS k
    ON k.region = s.region
   AND k.window_start_utc = s.period_start_utc
   AND k.window_end_utc   = s.period_end_utc;


-------------------------------------------------------------------------------
-- 8. Portfolio plane beginnings (biomass / pollinators)
-------------------------------------------------------------------------------

-- Portfolio-level ecological holdings across biomass and pollinator planes.
CREATE TABLE IF NOT EXISTS eco_portfolio_plane (
    portfolio_plane_id INTEGER PRIMARY KEY AUTOINCREMENT,
    portfolio_id       TEXT NOT NULL,                  -- matches StewardEcoWealthStatement.portfolio_id
    evolution_epoch_id INTEGER NOT NULL REFERENCES evolution_epoch(evolution_epoch_id) ON DELETE CASCADE,
    region             TEXT NOT NULL,
    -- Biomass and pollinator coordinates (normalized 0..1 for plane math, plus raw units).
    biomass_norm       REAL NOT NULL,
    biomass_raw        REAL NOT NULL,
    pollinator_norm    REAL NOT NULL,
    pollinator_raw     REAL NOT NULL,
    -- Derived eco-wealth metric combining biomass and pollinators.
    eco_wealth_score   REAL NOT NULL,
    -- Trust plane coordinate for this portfolio (0..1).
    r_trust_portfolio  REAL NOT NULL,
    -- Time window.
    period_start_utc   TEXT NOT NULL,
    period_end_utc     TEXT NOT NULL,
    created_utc        TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_eco_portfolio_plane_portfolio_epoch
    ON eco_portfolio_plane(portfolio_id, evolution_epoch_id, region);


-------------------------------------------------------------------------------
-- 9. Capability floor versioning and anti-dilution constraints
-------------------------------------------------------------------------------

-- Capability floor history per plane and epoch to prevent dilution.
CREATE TABLE IF NOT EXISTS capability_floor_history (
    floor_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    plane_id         INTEGER NOT NULL REFERENCES risk_plane(plane_id) ON DELETE CASCADE,
    evolution_epoch_id INTEGER NOT NULL REFERENCES evolution_epoch(evolution_epoch_id) ON DELETE CASCADE,
    -- Floor value for this plane and epoch (0..1).
    floor_value      REAL NOT NULL,
    -- Flag indicating this row passed the anti-dilution check in CI (monotone non-decreasing).
    monotone_ok      INTEGER NOT NULL DEFAULT 0 CHECK (monotone_ok IN (0,1)),
    created_utc      TEXT NOT NULL,
    UNIQUE (plane_id, evolution_epoch_id)
);

CREATE INDEX IF NOT EXISTS idx_capability_floor_plane_epoch
    ON capability_floor_history(plane_id, evolution_epoch_id);
