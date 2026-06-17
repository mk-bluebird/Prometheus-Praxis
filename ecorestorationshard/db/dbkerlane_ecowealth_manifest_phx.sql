-- filename: dbkerlane_ecowealth_manifest_phx.sql
-- destination: ecorestorationshard/db/dbkerlane_ecowealth_manifest_phx.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Canonical KER window index for Phoenix (snapshot + timeseries)
--    Wiring vshardker / vshardresidual into EcoUnit windows
----------------------------------------------------------------------

-- Per-shard KER windows, materialized as a table so they can be hashed
-- and referenced from StewardEcoWealthStatement and lane replay harnesses.
CREATE TABLE IF NOT EXISTS vshardkerwindow_phx (
    kerwindowid        INTEGER PRIMARY KEY AUTOINCREMENT,
    shardid            TEXT    NOT NULL,
    regioncode         TEXT    NOT NULL,  -- e.g. 'Phoenix-AZ-US'
    lane               TEXT    NOT NULL,  -- lane identifier from lanepolicy
    window_kind        TEXT    NOT NULL,  -- 'SHORT' | 'MEDIUM' | 'LONG'
    ts_start_utc       TEXT    NOT NULL,  -- inclusive window start
    ts_end_utc         TEXT    NOT NULL,  -- inclusive window end
    k_mean             REAL    NOT NULL,
    e_mean             REAL    NOT NULL,
    r_mean             REAL    NOT NULL,
    vt_max_window      REAL    NOT NULL,
    k_min              REAL    NOT NULL,
    k_max              REAL    NOT NULL,
    e_min              REAL    NOT NULL,
    e_max              REAL    NOT NULL,
    r_min              REAL    NOT NULL,
    r_max              REAL    NOT NULL,
    vt_min_window      REAL    NOT NULL,
    vt_trend           REAL    NOT NULL,  -- simple slope / normalized trend
    vshardkerwindowhash TEXT   NOT NULL,  -- hex hash over this row’s KER series
    shardlisthash       TEXT   NOT NULL,  -- hex hash over contributing shardids
    created_utc         TEXT   NOT NULL,
    UNIQUE (shardid, regioncode, lane, window_kind, ts_start_utc, ts_end_utc)
);

CREATE INDEX IF NOT EXISTS idx_vshardkerwindow_phx_region_lane
    ON vshardkerwindow_phx (regioncode, lane, window_kind, ts_end_utc);

CREATE INDEX IF NOT EXISTS idx_vshardkerwindow_phx_hash
    ON vshardkerwindow_phx (vshardkerwindowhash);


----------------------------------------------------------------------
-- 2. Canonical EcoUnit snapshot surface for Phoenix stewards
--    Deterministic function of KER windows + EcoWealthKernel2026v1
----------------------------------------------------------------------

-- EcoUnit snapshots are stored per steward, per KER window, for Phoenix.
-- These rows are non-actuating ledger state: they only describe crediting.
CREATE TABLE IF NOT EXISTS steward_ecounit_snapshot_phx (
    ecounitsnapshotid    INTEGER PRIMARY KEY AUTOINCREMENT,
    stewarddid           TEXT    NOT NULL,
    regioncode           TEXT    NOT NULL,  -- 'Phoenix-AZ-US'
    lane                 TEXT    NOT NULL,
    kerwindowid          INTEGER NOT NULL,
    ecowealthkernelid    TEXT    NOT NULL,  -- e.g. 'EcoWealthKernel2026v1'
    planecontractid      TEXT    NOT NULL,  -- e.g. 'PlaneWeightsShard2026v1'
    corridorsetid        TEXT    NOT NULL,
    lanepolicyid         TEXT    NOT NULL,
    k_mean               REAL    NOT NULL,
    e_mean               REAL    NOT NULL,
    r_mean               REAL    NOT NULL,
    vt_max_window        REAL    NOT NULL,
    governance_penalty   REAL    NOT NULL,  -- in [0,1]
    data_quality_factor  REAL    NOT NULL,  -- in [0,1]
    ws_scalar            REAL    NOT NULL,  -- Ws = E^α K^β (1−R)^γ
    ecounit_raw          REAL    NOT NULL,
    ecounit_final        REAL    NOT NULL,
    s_region             REAL    NOT NULL,  -- S_region multiplier
    b_steward            REAL    NOT NULL,  -- B_s multiplier
    trustdiscount_total  REAL    NOT NULL,  -- in [0,1]
    education_multiplier REAL    NOT NULL,
    representation_multiplier REAL NOT NULL,
    vshardkerwindowhash  TEXT    NOT NULL,
    shardlisthash        TEXT    NOT NULL,
    evidencehex          TEXT    NOT NULL,
    signingdid           TEXT    NOT NULL,  -- e.g. 'bostrom18sd2...'
    created_utc          TEXT    NOT NULL,
    UNIQUE (stewarddid, regioncode, lane, kerwindowid),
    FOREIGN KEY (kerwindowid) REFERENCES vshardkerwindow_phx (kerwindowid)
        ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_steward_ecounit_snapshot_phx_steward_day
    ON steward_ecounit_snapshot_phx (stewarddid, regioncode, lane, kerwindowid);

CREATE INDEX IF NOT EXISTS idx_steward_ecounit_snapshot_phx_kernel
    ON steward_ecounit_snapshot_phx (ecowealthkernelid, planecontractid);


-- Read-only view that aligns EcoUnit snapshots with KER window metadata.
CREATE VIEW IF NOT EXISTS vecounit_kerwindow_phx AS
SELECT
    s.ecounitsnapshotid,
    s.stewarddid,
    s.regioncode,
    s.lane,
    w.window_kind,
    w.ts_start_utc,
    w.ts_end_utc,
    s.k_mean,
    s.e_mean,
    s.r_mean,
    s.vt_max_window,
    w.vt_trend,
    s.governance_penalty,
    s.data_quality_factor,
    s.ws_scalar,
    s.ecounit_raw,
    s.ecounit_final,
    s.ecowealthkernelid,
    s.planecontractid,
    s.corridorsetid,
    s.lanepolicyid,
    s.vshardkerwindowhash,
    s.shardlisthash,
    s.evidencehex,
    s.signingdid
FROM steward_ecounit_snapshot_phx AS s
JOIN vshardkerwindow_phx         AS w
  ON s.kerwindowid = w.kerwindowid;


----------------------------------------------------------------------
-- 3. Lane promotion replay harness tables for Phoenix
--    RESEARCH → PROD non-regressive constraints over K,E,R,Vt
----------------------------------------------------------------------

-- Captures a single paired replay run over two corridor / lane snapshots
-- (old vs new) for Phoenix, including global tolerances and outcome.
CREATE TABLE IF NOT EXISTS lane_promotion_replay_run_phx (
    replayrunid      INTEGER PRIMARY KEY AUTOINCREMENT,
    regioncode       TEXT    NOT NULL,  -- 'Phoenix-AZ-US'
    scope_kind       TEXT    NOT NULL,  -- e.g. 'LANE', 'CORRIDORSET'
    scope_id_old     TEXT    NOT NULL,  -- lanepolicyid or corridorsetid (old)
    scope_id_new     TEXT    NOT NULL,  -- lanepolicyid or corridorsetid (new)
    lanescope        TEXT    NOT NULL,  -- lane or lane group
    eps_k            REAL    NOT NULL,
    eps_e            REAL    NOT NULL,
    eps_r            REAL    NOT NULL,
    eps_v            REAL    NOT NULL,
    roh_ceiling      REAL    NOT NULL,  -- e.g. 0.30
    started_utc      TEXT    NOT NULL,
    completed_utc    TEXT,
    replay_status    TEXT    NOT NULL,  -- 'PENDING'|'PASSED'|'FAILED'
    violation_count  INTEGER NOT NULL DEFAULT 0,
    evidencehex      TEXT    NOT NULL,
    signingdid       TEXT    NOT NULL,  -- e.g. 'bostrom18sd2...'
    UNIQUE (regioncode, scope_kind, scope_id_old, scope_id_new, lanescope, started_utc)
);

CREATE INDEX IF NOT EXISTS idx_lane_promotion_replay_run_phx_scope
    ON lane_promotion_replay_run_phx (regioncode, scope_kind, scope_id_old, scope_id_new, lanescope);


-- Per-shard, per-time-step replay results for a run, comparing K,E,R,Vt.
CREATE TABLE IF NOT EXISTS lane_promotion_replay_result_phx (
    replayresultid    INTEGER PRIMARY KEY AUTOINCREMENT,
    replayrunid       INTEGER NOT NULL,
    shardid           TEXT    NOT NULL,
    ts_end_utc        TEXT    NOT NULL,
    lane_old          TEXT    NOT NULL,
    lane_new          TEXT    NOT NULL,
    k_old             REAL    NOT NULL,
    k_new             REAL    NOT NULL,
    e_old             REAL    NOT NULL,
    e_new             REAL    NOT NULL,
    r_old             REAL    NOT NULL,
    r_new             REAL    NOT NULL,
    vt_old            REAL    NOT NULL,
    vt_new            REAL    NOT NULL,
    violates_k        INTEGER NOT NULL DEFAULT 0,
    violates_e        INTEGER NOT NULL DEFAULT 0,
    violates_r        INTEGER NOT NULL DEFAULT 0,
    violates_v        INTEGER NOT NULL DEFAULT 0,
    violates_lyapunov INTEGER NOT NULL DEFAULT 0,
    roh_old           REAL    NOT NULL,
    roh_new           REAL    NOT NULL,
    roh_ceiling       REAL    NOT NULL,
    roh_violation     INTEGER NOT NULL DEFAULT 0,
    violation_detail  TEXT,
    FOREIGN KEY (replayrunid)
        REFERENCES lane_promotion_replay_run_phx (replayrunid)
        ON DELETE CASCADE,
    UNIQUE (replayrunid, shardid, ts_end_utc)
);

CREATE INDEX IF NOT EXISTS idx_lane_promotion_replay_result_phx_run
    ON lane_promotion_replay_result_phx (replayrunid);

CREATE INDEX IF NOT EXISTS idx_lane_promotion_replay_result_phx_flags
    ON lane_promotion_replay_result_phx (
        violates_k,
        violates_e,
        violates_r,
        violates_v,
        violates_lyapunov,
        roh_violation
    );


-- View summarizing violation counts per replay run for fast CI checks.
CREATE VIEW IF NOT EXISTS vlane_promotion_replay_summary_phx AS
SELECT
    r.replayrunid,
    r.regioncode,
    r.scope_kind,
    r.scope_id_old,
    r.scope_id_new,
    r.lanescope,
    r.eps_k,
    r.eps_e,
    r.eps_r,
    r.eps_v,
    r.roh_ceiling,
    COUNT(*) AS rows_checked,
    SUM(CASE WHEN rr.violates_k        = 1 THEN 1 ELSE 0 END) AS k_violations,
    SUM(CASE WHEN rr.violates_e        = 1 THEN 1 ELSE 0 END) AS e_violations,
    SUM(CASE WHEN rr.violates_r        = 1 THEN 1 ELSE 0 END) AS r_violations,
    SUM(CASE WHEN rr.violates_v        = 1 THEN 1 ELSE 0 END) AS v_violations,
    SUM(CASE WHEN rr.violates_lyapunov = 1 THEN 1 ELSE 0 END) AS lyapunov_violations,
    SUM(CASE WHEN rr.roh_violation     = 1 THEN 1 ELSE 0 END) AS roh_violations
FROM lane_promotion_replay_run_phx    AS r
JOIN lane_promotion_replay_result_phx AS rr
  ON r.replayrunid = rr.replayrunid
GROUP BY
    r.replayrunid,
    r.regioncode,
    r.scope_kind,
    r.scope_id_old,
    r.scope_id_new,
    r.lanescope,
    r.eps_k,
    r.eps_e,
    r.eps_r,
    r.eps_v,
    r.roh_ceiling;


----------------------------------------------------------------------
-- 4. Unified daily evolution manifest for Phoenix
--    EcoUnit, KER, responsibility, RoH ≤ 0.30, representation floors
----------------------------------------------------------------------

-- Daily aggregates per steward and region, joinable to KER and EcoUnit.
CREATE TABLE IF NOT EXISTS steward_daily_state_phx (
    stewarddayid             INTEGER PRIMARY KEY AUTOINCREMENT,
    stewarddid               TEXT    NOT NULL,
    regioncode               TEXT    NOT NULL,  -- 'Phoenix-AZ-US'
    day_utc                  TEXT    NOT NULL,  -- YYYY-MM-DD
    k_mean_day               REAL    NOT NULL,
    e_mean_day               REAL    NOT NULL,
    r_mean_day               REAL    NOT NULL,
    vt_max_day               REAL    NOT NULL,
    ecounit_issued_day       REAL    NOT NULL,
    roh_max_day              REAL    NOT NULL,
    roh_ok                   INTEGER NOT NULL,  -- 1 if roh_max_day <= 0.30
    r_responsibility_day     REAL    NOT NULL,
    r_portfoliodiversity_day REAL    NOT NULL,
    r_topology_day           REAL    NOT NULL,
    governance_penalty_day   REAL    NOT NULL,
    data_quality_factor_day  REAL    NOT NULL,
    lifeforce_delta_day      REAL    NOT NULL,
    biokarma_delta_day       REAL    NOT NULL,
    representation_floor     REAL    NOT NULL,
    actual_ecowealth_share   REAL    NOT NULL,
    representation_ok        INTEGER NOT NULL,
    kerdeployable_day        INTEGER NOT NULL,  -- 1 only if all shards deployable
    lyapunov_ok_day          INTEGER NOT NULL,  -- 1 if no vshardkerviolation
    created_utc              TEXT    NOT NULL,
    UNIQUE (stewarddid, regioncode, day_utc)
);

CREATE INDEX IF NOT EXISTS idx_steward_daily_state_phx_region_day
    ON steward_daily_state_phx (regioncode, day_utc);

CREATE INDEX IF NOT EXISTS idx_steward_daily_state_phx_flags
    ON steward_daily_state_phx (roh_ok, representation_ok, kerdeployable_day, lyapunov_ok_day);


-- Unified, read-only daily manifest view with strict filters.
CREATE VIEW IF NOT EXISTS vunified_daily_evolution_manifest_phx AS
SELECT
    d.stewarddid,
    d.regioncode,
    d.day_utc,
    d.k_mean_day,
    d.e_mean_day,
    d.r_mean_day,
    d.vt_max_day,
    d.ecounit_issued_day,
    d.roh_max_day,
    d.roh_ok,
    d.r_responsibility_day,
    d.r_portfoliodiversity_day,
    d.r_topology_day,
    d.governance_penalty_day,
    d.data_quality_factor_day,
    d.lifeforce_delta_day,
    d.biokarma_delta_day,
    d.representation_floor,
    d.actual_ecowealth_share,
    d.representation_ok
FROM steward_daily_state_phx AS d
WHERE
    d.roh_ok            = 1   AND
    d.kerdeployable_day = 1   AND
    d.lyapunov_ok_day   = 1;


----------------------------------------------------------------------
-- 5. dbdefinitionregistry entries for these artifacts (Phoenix band)
----------------------------------------------------------------------

-- These INSERTs assume the existence of the standard EcoFort/EcoNet
-- definitionregistry and definitionscope tables. They anchor the
-- Phoenix-specific KER / lane / ecowealth surfaces under logical names.

INSERT OR IGNORE INTO definitionscope (scopeid, scopename, description)
VALUES
    ('KERWINDOW',      'KER window definitions',            'Canonical K,E,R,Vt windows per shard, region, lane'),
    ('ECOWEALTH',      'EcoWealth kernel + views',          'EcoUnit and StewardEcoWealthStatement wiring'),
    ('LANEREPLAY',     'Lane promotion replay policies',    'Replay-driven, non-regressive lane promotion checks'),
    ('EVOLUTIONDAILY', 'Daily evolution manifest surfaces', 'Unified daily KER, EcoUnit, RoH, representation views')
;

INSERT OR IGNORE INTO definitionregistry (
    logicalname,
    versiontag,
    hash,
    status,
    linkedtable,
    linkedaln,
    docpath
)
VALUES
    -- KER windows + EcoUnit snapshots (Phoenix)
    ('phx.vshardkerwindow.table.2026v1',
     '2026v1',
     lower(hex(randomblob(16))),
     'FROZENACTIVE',
     'vshardkerwindow_phx',
     'KerSnapshotWindow2026v1.aln',
     'docs/phx/ker/vshardkerwindow_phx.md'
    ),
    ('phx.steward.ecounit.snapshot.table.2026v1',
     '2026v1',
     lower(hex(randomblob(16))),
     'FROZENACTIVE',
     'steward_ecounit_snapshot_phx',
     'StewardEcoWealthStatement2026v1.aln',
     'docs/phx/ecowealth/steward_ecounit_snapshot_phx.md'
    ),
    ('phx.vecounit.kerwindow.view.2026v1',
     '2026v1',
     lower(hex(randomblob(16))),
     'FROZENACTIVE',
     'vecounit_kerwindow_phx',
     NULL,
     'docs/phx/ecowealth/vecounit_kerwindow_phx.md'
    ),

    -- Lane promotion replay surfaces (Phoenix)
    ('phx.lane.promotion.replay.run.table.2026v1',
     '2026v1',
     lower(hex(randomblob(16))),
     'FROZENACTIVE',
     'lane_promotion_replay_run_phx',
     'LanePromotionReplayPolicy2026v1.aln',
     'docs/phx/lane/lane_promotion_replay_run_phx.md'
    ),
    ('phx.lane.promotion.replay.result.table.2026v1',
     '2026v1',
     lower(hex(randomblob(16))),
     'FROZENACTIVE',
     'lane_promotion_replay_result_phx',
     'LanePromotionReplayViolation2026v1.aln',
     'docs/phx/lane/lane_promotion_replay_result_phx.md'
    ),
    ('phx.vlane.promotion.replay.summary.view.2026v1',
     '2026v1',
     lower(hex(randomblob(16))),
     'FROZENACTIVE',
     'vlane_promotion_replay_summary_phx',
     NULL,
     'docs/phx/lane/vlane_promotion_replay_summary_phx.md'
    ),

    -- Unified daily evolution manifest (Phoenix)
    ('phx.steward.daily.state.table.2026v1',
     '2026v1',
     lower(hex(randomblob(16))),
     'FROZENACTIVE',
     'steward_daily_state_phx',
     'StewardDailyStatePhoenix2026v1.aln',
     'docs/phx/evolution/steward_daily_state_phx.md'
    ),
    ('phx.vunified.daily.evolution.manifest.view.2026v1',
     '2026v1',
     lower(hex(randomblob(16))),
     'FROZENACTIVE',
     'vunified_daily_evolution_manifest_phx',
     'DailyEvolutionManifestPhoenix2026v1.aln',
     'docs/phx/evolution/vunified_daily_evolution_manifest_phx.md'
    )
;
