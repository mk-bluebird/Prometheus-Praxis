-- filename: db_phoenix_governance_spine.sql
-- destination: eco_restoration_shard/db/db_phoenix_governance_spine.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Universal KER windowing for Phoenix (replay + manifest)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS vshardkerwindow_phx (
    kerwindow_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    region              TEXT    NOT NULL,      -- "Phoenix-AZ"
    shard_id            TEXT    NOT NULL,      -- shardinstance.shardid
    ts_start_utc        TEXT    NOT NULL,
    ts_end_utc          TEXT    NOT NULL,
    k_mean              REAL    NOT NULL,
    k_min               REAL    NOT NULL,
    k_max               REAL    NOT NULL,
    e_mean              REAL    NOT NULL,
    e_min               REAL    NOT NULL,
    e_max               REAL    NOT NULL,
    r_mean              REAL    NOT NULL,
    r_min               REAL    NOT NULL,
    r_max               REAL    NOT NULL,
    vt_mean             REAL    NOT NULL,
    vt_min              REAL    NOT NULL,
    vt_max              REAL    NOT NULL,
    vshardkerwindowhash TEXT    NOT NULL,      -- deterministic hash over window rows
    author_bostrom      TEXT    NOT NULL,
    author_contract     TEXT    NOT NULL,
    created_utc         TEXT    NOT NULL
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_kerwindow_phx_unique
    ON vshardkerwindow_phx (region, shard_id, ts_start_utc, ts_end_utc);

CREATE TABLE IF NOT EXISTS steward_ecounit_snapshot_phx (
    snapshot_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    steward_id          TEXT    NOT NULL,
    region              TEXT    NOT NULL,      -- "Phoenix-AZ"
    kerwindow_id        INTEGER NOT NULL
                           REFERENCES vshardkerwindow_phx(kerwindow_id)
                           ON DELETE RESTRICT,
    eco_units_issued    REAL    NOT NULL,
    eco_units_penalty   REAL    NOT NULL,
    eco_units_adjusted  REAL    NOT NULL,
    eco_unit_hash       TEXT    NOT NULL,      -- hash over full calculation inputs
    roh                 REAL    NOT NULL,
    roh_ok              INTEGER NOT NULL CHECK (roh_ok IN (0,1)),
    created_utc         TEXT    NOT NULL,
    author_bostrom      TEXT    NOT NULL,
    author_contract     TEXT    NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_ecounit_phx_steward
    ON steward_ecounit_snapshot_phx (steward_id, region, created_utc);

----------------------------------------------------------------------
-- 2. Lane promotion replay run, result, violation (Phoenix)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS lane_promotion_replay_run_phx (
    run_id              INTEGER PRIMARY KEY AUTOINCREMENT,
    region              TEXT    NOT NULL,
    lane_from           TEXT    NOT NULL,      -- RESEARCH, EXPPROD
    lane_to             TEXT    NOT NULL,      -- EXPPROD, PROD
    snapshot_old_tag    TEXT    NOT NULL,      -- old KER corridor tag
    snapshot_new_tag    TEXT    NOT NULL,      -- new KER corridor tag
    eps_k               REAL    NOT NULL,
    eps_e               REAL    NOT NULL,
    eps_r               REAL    NOT NULL,
    eps_vt              REAL    NOT NULL,
    started_utc         TEXT    NOT NULL,
    finished_utc        TEXT,
    status              TEXT    NOT NULL CHECK (
                           status IN ('RUNNING','SUCCESS','FAILED')
                       ),
    author_bostrom      TEXT    NOT NULL,
    author_contract     TEXT    NOT NULL,
    routingspec_hex     TEXT    NOT NULL,
    notes               TEXT
);

CREATE TABLE IF NOT EXISTS lane_promotion_replay_result_phx (
    run_id              INTEGER NOT NULL
                           REFERENCES lane_promotion_replay_run_phx(run_id)
                           ON DELETE CASCADE,
    shard_id            TEXT    NOT NULL,
    ts_end_utc          TEXT    NOT NULL,
    k_old               REAL    NOT NULL,
    k_new               REAL    NOT NULL,
    e_old               REAL    NOT NULL,
    e_new               REAL    NOT NULL,
    r_old               REAL    NOT NULL,
    r_new               REAL    NOT NULL,
    vt_old              REAL    NOT NULL,
    vt_new              REAL    NOT NULL,
    v_violation         INTEGER NOT NULL DEFAULT 0 CHECK (v_violation IN (0,1)),
    k_violation         INTEGER NOT NULL DEFAULT 0 CHECK (k_violation IN (0,1)),
    e_violation         INTEGER NOT NULL DEFAULT 0 CHECK (e_violation IN (0,1)),
    r_violation         INTEGER NOT NULL DEFAULT 0 CHECK (r_violation IN (0,1)),
    PRIMARY KEY (run_id, shard_id, ts_end_utc)
);

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

CREATE INDEX IF NOT EXISTS idx_lane_replay_result_phx_run
    ON lane_promotion_replay_result_phx (run_id);

CREATE INDEX IF NOT EXISTS idx_lane_replay_violation_phx_run
    ON lane_promotion_replay_violation_phx (run_id);

----------------------------------------------------------------------
-- 3. Steward daily state + unified daily evolution manifest (view)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS steward_daily_state_phx (
    steward_id          TEXT    NOT NULL,
    region              TEXT    NOT NULL,
    day_utc             TEXT    NOT NULL,      -- YYYY-MM-DD
    eco_units_issued    REAL    NOT NULL,
    eco_units_penalty   REAL    NOT NULL,
    eco_units_adjusted  REAL    NOT NULL,
    roh                 REAL    NOT NULL,
    roh_ok              INTEGER NOT NULL CHECK (roh_ok IN (0,1)),
    ker_deployable      INTEGER NOT NULL CHECK (ker_deployable IN (0,1)),
    vt_violation        INTEGER NOT NULL CHECK (vt_violation IN (0,1)),
    dataquality_ok      INTEGER NOT NULL CHECK (dataquality_ok IN (0,1)),
    topology_ok         INTEGER NOT NULL CHECK (topology_ok IN (0,1)),
    representation_ok   INTEGER NOT NULL CHECK (representation_ok IN (0,1)),
    mt6883_ok           INTEGER NOT NULL CHECK (mt6883_ok IN (0,1)),
    roh_healthcare      REAL    NOT NULL,
    roh_healthcare_ok   INTEGER NOT NULL CHECK (roh_healthcare_ok IN (0,1)),
    created_utc         TEXT    NOT NULL,
    PRIMARY KEY (steward_id, region, day_utc)
);

DROP VIEW IF EXISTS vunified_daily_evolution_manifest_phx;

CREATE VIEW vunified_daily_evolution_manifest_phx AS
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
-- 4. Definition registry flags for Phoenix governance artifacts
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS definitionregistry_flags (
    defid               TEXT PRIMARY KEY,      -- definitionregistry.defid
    active              INTEGER NOT NULL CHECK (active IN (0,1)),
    monotone_ok         INTEGER NOT NULL CHECK (monotone_ok IN (0,1)),
    last_run_id         INTEGER,               -- lane_promotion_replay_run_phx.run_id or Kani run
    last_updated_utc    TEXT    NOT NULL,
    author_bostrom      TEXT    NOT NULL,
    author_contract     TEXT    NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_defregistry_flags_active
    ON definitionregistry_flags (active, monotone_ok);
