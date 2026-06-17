-- filename: db_ai_chat_metaview.sql
-- destination: eco_restoration_shard/db/db_ai_chat_metaview.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Steward / node identity spine for AI-chat meta-views
--
-- This is a lightweight, read-only indexing layer that binds stewards
-- (DIDs, Bostrom addresses) and nodes to the constellation spine
-- (shardinstance, knowledgeecoscore, stake, eco_wealth, healthcare, etc.).
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS ai_chat_steward_index (
    steward_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    steward_did        TEXT    NOT NULL,  -- DID or host DID
    bostrom_address    TEXT,              -- e.g. bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
    display_name       TEXT,              -- short human label for AI-chat
    region             TEXT,              -- home region (Phoenix-AZ, Global)
    primary_roleband   TEXT,              -- SPINE, RESEARCH, ENGINE, MATERIAL, GOV, APP
    created_utc        TEXT    NOT NULL,
    updated_utc        TEXT    NOT NULL,
    UNIQUE (steward_did),
    UNIQUE (bostrom_address)
);

CREATE TABLE IF NOT EXISTS ai_chat_node_index (
    node_index_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    nodeid             TEXT    NOT NULL,  -- matches shardinstance.nodeid
    region             TEXT    NOT NULL,
    primary_plane      TEXT,              -- hydraulics, energy, healthcare, etc.
    roleband           TEXT,              -- SPINE, ENGINE, MATERIAL, etc.
    created_utc        TEXT    NOT NULL,
    updated_utc        TEXT    NOT NULL,
    UNIQUE (nodeid)
);

CREATE INDEX IF NOT EXISTS idx_ai_chat_steward_region
    ON ai_chat_steward_index (region, primary_roleband);

CREATE INDEX IF NOT EXISTS idx_ai_chat_node_region_plane
    ON ai_chat_node_index (region, primary_plane);


-------------------------------------------------------------------------------
-- 2. Eco-wealth meta-index
--
-- This table records per-steward and per-node eco-wealth summaries over
-- configurable windows, derived from knowledgeecoscore, Cyboquatic views,
-- rewards ledgers, and energy cost metrics.
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS ai_chat_eco_wealth_summary (
    eco_wealth_id      INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Scope
    scope_level        TEXT    NOT NULL,  -- STEWARD, NODE
    steward_did        TEXT,              -- used when scope_level = 'STEWARD'
    nodeid             TEXT,              -- used when scope_level = 'NODE'
    region             TEXT    NOT NULL,

    -- Time window
    window_start_utc   TEXT    NOT NULL,
    window_end_utc     TEXT    NOT NULL,

    -- Aggregated KER metrics (normalized 0..1)
    k_avg              REAL,
    e_avg              REAL,
    r_avg              REAL,

    -- Eco-wealth metrics
    eco_wealth_score   REAL,             -- composite eco-wealth index 0..1
    eco_gain_per_joule REAL,             -- ecoperjoule style metric
    carbon_negative_ok INTEGER NOT NULL DEFAULT 0 CHECK (carbon_negative_ok IN (0,1)),

    -- Rewards and stake
    total_rewards      REAL,             -- total eco-reward tokens over window
    total_stake        REAL,             -- total stake / commitment value
    rewards_events     INTEGER,          -- count of reward events
    stake_events       INTEGER,          -- count of stake events

    -- Meta
    issuing_system     TEXT    NOT NULL, -- e.g. VirtaSys, EcoWealthCartographer
    created_utc        TEXT    NOT NULL,
    updated_utc        TEXT    NOT NULL,

    CHECK (eco_wealth_score IS NULL OR (eco_wealth_score >= 0.0 AND eco_wealth_score <= 1.0)),
    CHECK (k_avg IS NULL OR (k_avg >= 0.0 AND k_avg <= 1.0)),
    CHECK (e_avg IS NULL OR (e_avg >= 0.0 AND e_avg <= 1.0)),
    CHECK (r_avg IS NULL OR (r_avg >= 0.0 AND r_avg <= 1.0)),

    UNIQUE (scope_level, COALESCE(steward_did, ''), COALESCE(nodeid, ''), window_start_utc, window_end_utc)
);

CREATE INDEX IF NOT EXISTS idx_ai_chat_eco_wealth_scope
    ON ai_chat_eco_wealth_summary (scope_level, steward_did, nodeid, window_end_utc);

CREATE INDEX IF NOT EXISTS idx_ai_chat_eco_wealth_region
    ON ai_chat_eco_wealth_summary (region, window_end_utc);

CREATE INDEX IF NOT EXISTS idx_ai_chat_eco_wealth_score
    ON ai_chat_eco_wealth_summary (eco_wealth_score DESC);


-------------------------------------------------------------------------------
-- 3. Responsibility and healthcare meta-index
--
-- These tables normalize responsibilitymetric and healthcare KER / RoH
-- summaries into a unified AI-chat facing shape.
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS ai_chat_responsibility_summary (
    responsibility_meta_id INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Scope
    scope_level           TEXT    NOT NULL,   -- STEWARD, NODE
    steward_did           TEXT,
    nodeid                TEXT,
    region                TEXT    NOT NULL,

    window_start_utc      TEXT    NOT NULL,
    window_end_utc        TEXT    NOT NULL,

    -- Responsibility axes (0..1)
    energy_duty_score     REAL,
    ecoimpact_score       REAL,
    lifeforce_stewardship REAL,
    roh_stewardship       REAL,
    topology_stewardship  REAL,
    data_quality_score    REAL,
    restoration_duty_score REAL,
    justice_duty_score    REAL,

    responsibility_index  REAL,

    -- Meta
    responsibility_model  TEXT    NOT NULL,    -- e.g. ResponsibilityAxis2026v1
    issuing_system        TEXT    NOT NULL,
    created_utc           TEXT    NOT NULL,
    updated_utc           TEXT    NOT NULL,

    CHECK (responsibility_index IS NULL OR (responsibility_index >= 0.0 AND responsibility_index <= 1.0)),

    UNIQUE (scope_level, COALESCE(steward_did, ''), COALESCE(nodeid, ''), window_start_utc, window_end_utc)
);

CREATE INDEX IF NOT EXISTS idx_ai_chat_responsibility_scope
    ON ai_chat_responsibility_summary (scope_level, steward_did, nodeid, window_end_utc);

CREATE INDEX IF NOT EXISTS idx_ai_chat_responsibility_index
    ON ai_chat_responsibility_summary (responsibility_index DESC);


CREATE TABLE IF NOT EXISTS ai_chat_healthcare_summary (
    healthcare_meta_id     INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Scope (host / steward / node)
    steward_did            TEXT,
    host_did               TEXT,
    nodeid                 TEXT,
    region                 TEXT    NOT NULL,

    window_start_utc       TEXT    NOT NULL,
    window_end_utc         TEXT    NOT NULL,

    -- RoH / healthcare KER axes
    roh_avg                REAL,   -- average RoH over window
    roh_max                REAL,
    lifeforce_avg          REAL,
    lifeforce_floor_breaches INTEGER NOT NULL DEFAULT 0,
    ecostress_avg          REAL,
    ecostress_max          REAL,

    -- Course and lane metrics
    detox_sessions         INTEGER,
    holidays_enforced      INTEGER,
    medical_hold_windows   INTEGER,
    healthcare_lane_label  TEXT,   -- HIGHINTENSITY, MAINTENANCEONLY, MEDICALHOLD

    -- Compliance flags
    roh_safe               INTEGER NOT NULL DEFAULT 0 CHECK (roh_safe IN (0,1)),
    neurorights_safe       INTEGER NOT NULL DEFAULT 0 CHECK (neurorights_safe IN (0,1)),
    eco_compliant          INTEGER NOT NULL DEFAULT 0 CHECK (eco_compliant IN (0,1)),

    issuing_system         TEXT    NOT NULL,
    created_utc            TEXT    NOT NULL,
    updated_utc            TEXT    NOT NULL,

    CHECK (roh_avg IS NULL OR (roh_avg >= 0.0 AND roh_avg <= 1.0)),
    CHECK (roh_max IS NULL OR (roh_max >= 0.0 AND roh_max <= 1.0)),
    CHECK (lifeforce_avg IS NULL OR (lifeforce_avg >= 0.0 AND lifeforce_avg <= 1.0))
);

CREATE INDEX IF NOT EXISTS idx_ai_chat_healthcare_scope
    ON ai_chat_healthcare_summary (steward_did, host_did, nodeid, window_end_utc);

CREATE INDEX IF NOT EXISTS idx_ai_chat_healthcare_roh
    ON ai_chat_healthcare_summary (roh_avg, roh_max);


-------------------------------------------------------------------------------
-- 4. Stake and reward meta-index
--
-- This table aggregates stake, reward, and portfolio KER for AI-chat.
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS ai_chat_stake_reward_summary (
    stake_meta_id       INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Scope
    steward_did         TEXT    NOT NULL,
    region              TEXT    NOT NULL,

    window_start_utc    TEXT    NOT NULL,
    window_end_utc      TEXT    NOT NULL,

    -- Stake metrics
    total_stake_value   REAL,
    stake_positions     INTEGER,

    -- Reward metrics
    total_reward_tokens REAL,
    reward_events       INTEGER,

    -- Portfolio KER metrics
    portfolio_k_avg     REAL,
    portfolio_e_avg     REAL,
    portfolio_r_avg     REAL,

    -- Governance / risk flags
    topology_risk_avg   REAL,
    nonactuating_share  REAL,   -- fraction of workloads nonactuating
    prod_lane_share     REAL,   -- fraction of workloads in PROD lane

    issuing_system      TEXT    NOT NULL,
    created_utc         TEXT    NOT NULL,
    updated_utc         TEXT    NOT NULL,

    CHECK (portfolio_k_avg IS NULL OR (portfolio_k_avg >= 0.0 AND portfolio_k_avg <= 1.0)),
    CHECK (portfolio_e_avg IS NULL OR (portfolio_e_avg >= 0.0 AND portfolio_e_avg <= 1.0)),
    CHECK (portfolio_r_avg IS NULL OR (portfolio_r_avg >= 0.0 AND portfolio_r_avg <= 1.0))
);

CREATE INDEX IF NOT EXISTS idx_ai_chat_stake_scope
    ON ai_chat_stake_reward_summary (steward_did, region, window_end_utc);


-------------------------------------------------------------------------------
-- 5. AI-chat meta-view over constellation spine
--
-- These views join the above meta-index tables into a single, read-only
-- surface per steward or per node, so AI-chat can answer questions like:
-- "Show my eco-wealth, KER, responsibility, healthcare, and stake history."
-------------------------------------------------------------------------------

-- Assumptions:
--   - ecosafety / KER core lives in ecosafetygrammarcore.sql
--   - shardinstance, knowledgeecoscore, and lane / stake tables exist
--   - eco_wealth, reward, and responsibility metrics are populated by CI

DROP VIEW IF EXISTS v_ai_chat_meta_steward;
CREATE VIEW v_ai_chat_meta_steward AS
SELECT
    s.steward_did,
    s.bostrom_address,
    s.display_name,
    s.region,
    s.primary_roleband,

    -- Eco-wealth (latest window per steward)
    ew.window_start_utc          AS ew_window_start_utc,
    ew.window_end_utc            AS ew_window_end_utc,
    ew.k_avg                     AS ew_k_avg,
    ew.e_avg                     AS ew_e_avg,
    ew.r_avg                     AS ew_r_avg,
    ew.eco_wealth_score,
    ew.eco_gain_per_joule,
    ew.carbon_negative_ok,
    ew.total_rewards             AS ew_total_rewards,
    ew.total_stake               AS ew_total_stake,

    -- Responsibility (latest window per steward)
    rs.window_start_utc          AS resp_window_start_utc,
    rs.window_end_utc            AS resp_window_end_utc,
    rs.energy_duty_score,
    rs.ecoimpact_score,
    rs.lifeforce_stewardship,
    rs.roh_stewardship,
    rs.topology_stewardship,
    rs.data_quality_score,
    rs.restoration_duty_score,
    rs.justice_duty_score,
    rs.responsibility_index,

    -- Healthcare (latest window per steward)
    hc.window_start_utc          AS hc_window_start_utc,
    hc.window_end_utc            AS hc_window_end_utc,
    hc.roh_avg,
    hc.roh_max,
    hc.lifeforce_avg,
    hc.lifeforce_floor_breaches,
    hc.ecostress_avg,
    hc.ecostress_max,
    hc.detox_sessions,
    hc.holidays_enforced,
    hc.medical_hold_windows,
    hc.healthcare_lane_label,
    hc.roh_safe,
    hc.neurorights_safe,
    hc.eco_compliant,

    -- Stake / reward portfolio (latest window)
    sr.window_start_utc          AS stake_window_start_utc,
    sr.window_end_utc            AS stake_window_end_utc,
    sr.total_stake_value,
    sr.stake_positions,
    sr.total_reward_tokens,
    sr.reward_events,
    sr.portfolio_k_avg,
    sr.portfolio_e_avg,
    sr.portfolio_r_avg,
    sr.topology_risk_avg,
    sr.nonactuating_share,
    sr.prod_lane_share

FROM ai_chat_steward_index AS s

LEFT JOIN ai_chat_eco_wealth_summary AS ew
    ON ew.scope_level = 'STEWARD'
   AND ew.steward_did = s.steward_did
   AND ew.window_end_utc = (
       SELECT MAX(window_end_utc)
       FROM ai_chat_eco_wealth_summary
       WHERE scope_level = 'STEWARD'
         AND steward_did = s.steward_did
   )

LEFT JOIN ai_chat_responsibility_summary AS rs
    ON rs.scope_level = 'STEWARD'
   AND rs.steward_did = s.steward_did
   AND rs.window_end_utc = (
       SELECT MAX(window_end_utc)
       FROM ai_chat_responsibility_summary
       WHERE scope_level = 'STEWARD'
         AND steward_did = s.steward_did
   )

LEFT JOIN ai_chat_healthcare_summary AS hc
    ON hc.steward_did = s.steward_did
   AND hc.window_end_utc = (
       SELECT MAX(window_end_utc)
       FROM ai_chat_healthcare_summary
       WHERE steward_did = s.steward_did
   )

LEFT JOIN ai_chat_stake_reward_summary AS sr
    ON sr.steward_did = s.steward_did
   AND sr.window_end_utc = (
       SELECT MAX(window_end_utc)
       FROM ai_chat_stake_reward_summary
       WHERE steward_did = s.steward_did
   );


DROP VIEW IF NOT EXISTS v_ai_chat_meta_node;
CREATE VIEW v_ai_chat_meta_node AS
SELECT
    n.nodeid,
    n.region,
    n.primary_plane,
    n.roleband,

    -- Eco-wealth (latest node window)
    ew.window_start_utc          AS ew_window_start_utc,
    ew.window_end_utc            AS ew_window_end_utc,
    ew.k_avg                     AS ew_k_avg,
    ew.e_avg                     AS ew_e_avg,
    ew.r_avg                     AS ew_r_avg,
    ew.eco_wealth_score,
    ew.eco_gain_per_joule,
    ew.carbon_negative_ok,
    ew.total_rewards             AS ew_total_rewards,
    ew.total_stake               AS ew_total_stake,

    -- Responsibility (latest node window)
    rs.window_start_utc          AS resp_window_start_utc,
    rs.window_end_utc            AS resp_window_end_utc,
    rs.energy_duty_score,
    rs.ecoimpact_score,
    rs.lifeforce_stewardship,
    rs.roh_stewardship,
    rs.topology_stewardship,
    rs.data_quality_score,
    rs.restoration_duty_score,
    rs.justice_duty_score,
    rs.responsibility_index,

    -- Healthcare (latest node window)
    hc.window_start_utc          AS hc_window_start_utc,
    hc.window_end_utc            AS hc_window_end_utc,
    hc.roh_avg,
    hc.roh_max,
    hc.lifeforce_avg,
    hc.lifeforce_floor_breaches,
    hc.ecostress_avg,
    hc.ecostress_max,
    hc.detox_sessions,
    hc.holidays_enforced,
    hc.medical_hold_windows,
    hc.healthcare_lane_label,
    hc.roh_safe,
    hc.neurorights_safe,
    hc.eco_compliant

FROM ai_chat_node_index AS n

LEFT JOIN ai_chat_eco_wealth_summary AS ew
    ON ew.scope_level = 'NODE'
   AND ew.nodeid = n.nodeid
   AND ew.window_end_utc = (
       SELECT MAX(window_end_utc)
       FROM ai_chat_eco_wealth_summary
       WHERE scope_level = 'NODE'
         AND nodeid = n.nodeid
   )

LEFT JOIN ai_chat_responsibility_summary AS rs
   ON rs.scope_level = 'NODE'
  AND rs.nodeid = n.nodeid
  AND rs.window_end_utc = (
       SELECT MAX(window_end_utc)
       FROM ai_chat_responsibility_summary
       WHERE scope_level = 'NODE'
         AND nodeid = n.nodeid
   )

LEFT JOIN ai_chat_healthcare_summary AS hc
   ON hc.nodeid = n.nodeid
  AND hc.window_end_utc = (
       SELECT MAX(window_end_utc)
       FROM ai_chat_healthcare_summary
       WHERE nodeid = n.nodeid
   );


-------------------------------------------------------------------------------
-- 6. DefinitionRegistry entry hint (for EcoFort / EcoNet)
--
-- This table is optional; it documents how AI-chat meta-views are registered
-- in the constellation-wide DefinitionRegistry for discoverability.
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS ai_chat_metaview_definition (
    def_id             INTEGER PRIMARY KEY AUTOINCREMENT,
    logical_name       TEXT    NOT NULL,  -- e.g. ai.chat.meta.steward.2026v1
    description        TEXT    NOT NULL,
    db_path            TEXT    NOT NULL,  -- relative path to DB file
    view_name          TEXT    NOT NULL,  -- v_ai_chat_meta_steward / v_ai_chat_meta_node
    contract_id        TEXT    NOT NULL,  -- DefinitionRegistry contract id
    author_bostrom     TEXT,
    created_utc        TEXT    NOT NULL,
    updated_utc        TEXT    NOT NULL,
    UNIQUE (logical_name, view_name)
);

CREATE INDEX IF NOT EXISTS idx_ai_chat_metaview_def_name
    ON ai_chat_metaview_definition (logical_name);
