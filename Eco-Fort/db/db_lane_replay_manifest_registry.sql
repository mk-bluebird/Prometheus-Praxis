-- filename: db_lane_replay_manifest_registry.sql
-- destination: Eco-Fort/db/db_lane_replay_manifest_registry.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Lane promotion replay run + violation logging (Phoenix)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS lane_promotion_replay_run_phx (
    run_id              INTEGER PRIMARY KEY AUTOINCREMENT,
    region              TEXT    NOT NULL,         -- e.g. "Phoenix-AZ"
    lane_from           TEXT    NOT NULL,         -- e.g. "RESEARCH"
    lane_to             TEXT    NOT NULL,         -- e.g. "PROD"
    snapshot_old_tag    TEXT    NOT NULL,         -- e.g. "PhoenixESPD-2026v1"
    snapshot_new_tag    TEXT    NOT NULL,         -- e.g. "PhoenixESPD-2026v2"
    eps_k               REAL    NOT NULL,         -- tolerance for K
    eps_e               REAL    NOT NULL,         -- tolerance for E
    eps_r               REAL    NOT NULL,         -- tolerance for R
    eps_vt              REAL    NOT NULL,         -- tolerance for Vt_with_topology
    started_utc         TEXT    NOT NULL,         -- ISO8601 UTC
    finished_utc        TEXT,                     -- NULL if still running
    status              TEXT    NOT NULL CHECK(
                              status IN ('RUNNING','SUCCESS','FAILED')
                         ),
    author_bostrom      TEXT    NOT NULL,         -- e.g. "bostrom18..."
    author_contract     TEXT    NOT NULL,         -- e.g. "LaneReplayPolicy2026v1"
    routingspec_hex     TEXT    NOT NULL,         -- hex of governance spec used
    notes               TEXT
);

CREATE TABLE IF NOT EXISTS lane_promotion_replay_result_phx (
    run_id              INTEGER NOT NULL
                               REFERENCES lane_promotion_replay_run_phx(run_id)
                               ON DELETE CASCADE,
    shard_id            TEXT    NOT NULL,         -- shardinstance.shardid
    ts_end_utc          TEXT    NOT NULL,         -- shardinstance.tsendutc
    k_old               REAL    NOT NULL,
    k_new               REAL    NOT NULL,
    e_old               REAL    NOT NULL,
    e_new               REAL    NOT NULL,
    r_old               REAL    NOT NULL,
    r_new               REAL    NOT NULL,
    vt_old              REAL    NOT NULL,         -- Vt_with_topology old
    vt_new              REAL    NOT NULL,         -- Vt_with_topology new
    v_violation         INTEGER NOT NULL DEFAULT 0 CHECK (v_violation IN (0,1)),
    k_violation         INTEGER NOT NULL DEFAULT 0 CHECK (k_violation IN (0,1)),
    e_violation         INTEGER NOT NULL DEFAULT 0 CHECK (e_violation IN (0,1)),
    r_violation         INTEGER NOT NULL DEFAULT 0 CHECK (r_violation IN (0,1)),
    PRIMARY KEY (run_id, shard_id, ts_end_utc)
);

-- Strict, immutable record of any failed inequality for governance review.
CREATE TABLE IF NOT EXISTS lane_promotion_replay_violation_phx (
    violation_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id              INTEGER NOT NULL
                               REFERENCES lane_promotion_replay_run_phx(run_id)
                               ON DELETE CASCADE,
    shard_id            TEXT    NOT NULL,
    ts_end_utc          TEXT    NOT NULL,
    violation_type      TEXT    NOT NULL CHECK (
                              violation_type IN (
                                  'K_REGRESSION',
                                  'E_REGRESSION',
                                  'R_INCREASE',
                                  'VT_REGRESSION',
                                  'NEW_VIOLATION_ROW'
                              )
                         ),
    k_old               REAL,
    k_new               REAL,
    e_old               REAL,
    e_new               REAL,
    r_old               REAL,
    r_new               REAL,
    vt_old              REAL,
    vt_new              REAL,
    created_utc         TEXT    NOT NULL,
    author_bostrom      TEXT    NOT NULL,
    author_contract     TEXT    NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_lane_replay_result_run
    ON lane_promotion_replay_result_phx (run_id);

CREATE INDEX IF NOT EXISTS idx_lane_replay_violation_run
    ON lane_promotion_replay_violation_phx (run_id);

----------------------------------------------------------------------
-- 2. Unified daily evolution manifest (Phoenix view)
----------------------------------------------------------------------

-- Base daily state table: steward_daily_state_phx
-- (Assumed populated by existing Phoenix evolution loop.)
CREATE TABLE IF NOT EXISTS steward_daily_state_phx (
    steward_id          TEXT    NOT NULL,
    region              TEXT    NOT NULL,
    day_utc             TEXT    NOT NULL,         -- YYYY-MM-DD
    eco_units_issued    REAL    NOT NULL,
    eco_units_penalty   REAL    NOT NULL,
    eco_units_adjusted  REAL    NOT NULL,
    roh                 REAL    NOT NULL,         -- Risk-of-Harm [0,1]
    roh_ok              INTEGER NOT NULL CHECK (roh_ok IN (0,1)),
    ker_deployable      INTEGER NOT NULL CHECK (ker_deployable IN (0,1)),
    vt_violation        INTEGER NOT NULL CHECK (vt_violation IN (0,1)),
    dataquality_ok      INTEGER NOT NULL CHECK (dataquality_ok IN (0,1)),
    topology_ok         INTEGER NOT NULL CHECK (topology_ok IN (0,1)),
    representation_ok   INTEGER NOT NULL CHECK (representation_ok IN (0,1)),
    mt6883_ok           INTEGER NOT NULL CHECK (mt6883_ok IN (0,1)),
    roh_healthcare      REAL    NOT NULL,         -- healthcare RoH coordinate
    roh_healthcare_ok   INTEGER NOT NULL CHECK (roh_healthcare_ok IN (0,1)),
    created_utc         TEXT    NOT NULL,
    PRIMARY KEY (steward_id, region, day_utc)
);

-- Read-only manifest that preserves all rows; flags only.
CREATE VIEW IF NOT EXISTS vunified_daily_evolution_manifest_phx AS
SELECT
    s.steward_id,
    s.region,
    s.day_utc,
    s.eco_units_issued,
    s.eco_units_penalty,
    s.eco_units_adjusted,
    s.roh,
    s.roh_ok,
    s.ker_deployable,
    s.vt_violation,
    s.dataquality_ok,
    s.topology_ok,
    s.representation_ok,
    s.mt6883_ok,
    s.roh_healthcare,
    s.roh_healthcare_ok
FROM steward_daily_state_phx AS s;

----------------------------------------------------------------------
-- 3. Definition registry flags for replay + manifest + registry
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS definitionregistry_flags (
    def_id         TEXT PRIMARY KEY,             -- matches definitionregistry.defid
    active         INTEGER NOT NULL CHECK (active IN (0,1)),
    monotone_ok    INTEGER NOT NULL CHECK (monotone_ok IN (0,1)),
    last_run_id    INTEGER,                      -- last successful replay or Kani run
    last_updated   TEXT NOT NULL,                -- ISO8601 UTC
    author_bostrom TEXT NOT NULL,
    author_contract TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_defregistry_flags_active
    ON definitionregistry_flags (active, monotone_ok);
