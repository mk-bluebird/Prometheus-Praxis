-- filename: econet_mcp/db/ecosphere_reward_index.sql
-- destination: EcoNet/econet_mcp/db/ecosphere_reward_index.sql
-- purpose: biocompatible EcoNet rewards and distributions surface, wired to MT6883 registry and governance spine

PRAGMA foreign_keys = ON;

------------------------------------------------------------
-- 1. Core reward planes and MT6883 host profile bindings
------------------------------------------------------------

CREATE TABLE IF NOT EXISTS reward_plane (
    plane_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    planecode       TEXT NOT NULL UNIQUE,         -- e.g. 'HEALTHCARE', 'HYDRO_CLEANUP', 'CARBON_NEG', 'CITIZEN_GOV'
    description     TEXT NOT NULL,
    nonoffsettable  INTEGER NOT NULL DEFAULT 0,   -- 1 means may not be compensated by other planes
    weight          REAL NOT NULL DEFAULT 1.0,    -- relative weight in reward aggregation, KER-aligned
    CHECK (weight >= 0.0 AND weight <= 10.0),
    CHECK (nonoffsettable IN (0,1))
);

CREATE TABLE IF NOT EXISTS mt6883_host_profile (
    host_profile_id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_code    TEXT NOT NULL UNIQUE,        -- e.g. 'MT6883_MEDICAL_AZ', 'MT6883_IOT_PHOENIX'
    description     TEXT NOT NULL,
    continuity_min  TEXT NOT NULL,               -- 'A','B','C' as encoded in mt6883registry continuitygrade
    lane_min        TEXT NOT NULL,               -- minimal lane allowed to earn rewards, e.g. 'EXPPROD' or 'PROD'
    roh_ceiling     REAL NOT NULL DEFAULT 0.30,  -- risk-of-harm ceiling for rewardable work
    CHECK (roh_ceiling >= 0.0 AND roh_ceiling <= 1.0)
);

CREATE TABLE IF NOT EXISTS mt6883_profile_binding (
    binding_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    host_profile_id INTEGER NOT NULL REFERENCES mt6883_host_profile(host_profile_id),
    nodeid          TEXT NOT NULL,              -- must correspond to mt6883registry.nodeid
    region          TEXT NOT NULL,
    active          INTEGER NOT NULL DEFAULT 1,
    UNIQUE (nodeid, region),
    CHECK (active IN (0,1))
);

------------------------------------------------------------
-- 2. Reward windows keyed to KER / ecosafety windows
------------------------------------------------------------

CREATE TABLE IF NOT EXISTS reward_window_family (
    reward_family_id  INTEGER PRIMARY KEY AUTOINCREMENT,
    family_code       TEXT NOT NULL UNIQUE,     -- e.g. 'HEALTHCARE_M30', 'ECO_CLEANUP_D7'
    description       TEXT NOT NULL,
    window_seconds    INTEGER NOT NULL,        -- length of window for accumulation
    lane_min          TEXT NOT NULL,           -- minimal lane for eligibility
    k_min             REAL NOT NULL DEFAULT 0.90,
    e_min             REAL NOT NULL DEFAULT 0.90,
    r_max             REAL NOT NULL DEFAULT 0.13,
    CHECK (window_seconds > 0),
    CHECK (k_min >= 0.0 AND k_min <= 1.0),
    CHECK (e_min >= 0.0 AND e_min <= 1.0),
    CHECK (r_max >= 0.0 AND r_max <= 1.0)
);

CREATE TABLE IF NOT EXISTS reward_plane_family (
    plane_family_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    reward_family_id  INTEGER NOT NULL REFERENCES reward_window_family(reward_family_id),
    plane_id          INTEGER NOT NULL REFERENCES reward_plane(plane_id),
    share             REAL NOT NULL DEFAULT 1.0,   -- relative share of this plane inside the family
    mandatory         INTEGER NOT NULL DEFAULT 1,  -- 1: plane must be non-worsening in window to earn
    UNIQUE (reward_family_id, plane_id),
    CHECK (share >= 0.0 AND share <= 10.0),
    CHECK (mandatory IN (0,1))
);

------------------------------------------------------------
-- 3. Bostrom / token sink mappings for biocompatible payout
------------------------------------------------------------

CREATE TABLE IF NOT EXISTS reward_token_sink (
    sink_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    sink_code        TEXT NOT NULL UNIQUE,       -- e.g. 'BOSTROM_PRIMARY', 'BOSTROM_SAFE_ALT_1'
    chain_kind       TEXT NOT NULL,             -- e.g. 'BOSTROM', 'EVM'
    address          TEXT NOT NULL,             -- bech32 / hex, validated by off-chain tooling
    description      TEXT NOT NULL,
    active           INTEGER NOT NULL DEFAULT 1,
    CHECK (active IN (0,1))
);

INSERT OR IGNORE INTO reward_token_sink (sink_code, chain_kind, address, description, active)
VALUES
    ('BOSTROM_PRIMARY',   'BOSTROM', 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     'Primary Bostrom address for augmented-citizen eco-health rewards', 1),
    ('BOSTROM_ALT_SECURE','BOSTROM', 'bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc',
     'Secure alternate Bostrom address, monitored via RT channel', 1),
    ('BOSTROM_SAFE_ZETA', 'BOSTROM', 'zeta12x0up66pzyeretzyku8p4ccuxrjqtqpdc4y4x8',
     'Safe alternate Bostrom-compatible address (Zeta)', 1),
    ('ERC20_WALLET_MAIN', 'EVM',     '0x519fC0eB4111323Cac44b70e1aE31c30e405802D',
     'EVM-compatible wallet for ERC-20 mirrored eco-reward tokens', 1);

CREATE TABLE IF NOT EXISTS reward_identity_binding (
    identity_binding_id INTEGER PRIMARY KEY AUTOINCREMENT,
    citizen_did         TEXT NOT NULL,           -- e.g. Bostrom DID / ALN identity for citizen
    sink_id             INTEGER NOT NULL REFERENCES reward_token_sink(sink_id),
    preferred           INTEGER NOT NULL DEFAULT 0,
    created_ms          INTEGER NOT NULL,        -- ms since epoch
    UNIQUE (citizen_did, sink_id),
    CHECK (preferred IN (0,1))
);

------------------------------------------------------------
-- 4. MT6883 execution → reward evidence, biocompatible
------------------------------------------------------------

-- This table links MT6883 execution windows, ecosafety KER snapshots,
-- and reward planes into a single normalized surface that MCP tools
-- can query without touching actuation paths.

CREATE TABLE IF NOT EXISTS reward_execution_window (
    reward_window_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    nodeid             TEXT NOT NULL,            -- MT6883 node id
    region             TEXT NOT NULL,
    host_profile_id    INTEGER NOT NULL REFERENCES mt6883_host_profile(host_profile_id),
    reward_family_id   INTEGER NOT NULL REFERENCES reward_window_family(reward_family_id),
    window_start_ms    INTEGER NOT NULL,
    window_end_ms      INTEGER NOT NULL,
    lane               TEXT NOT NULL,            -- from lanestatusshard / mt6883registry
    k_avg              REAL NOT NULL,
    e_avg              REAL NOT NULL,
    r_avg              REAL NOT NULL,
    vt_max             REAL NOT NULL,
    roh_est            REAL NOT NULL DEFAULT 0.0,
    lane_ok            INTEGER NOT NULL DEFAULT 0,  -- derived from LaneStatusShard gating
    continuity_ok      INTEGER NOT NULL DEFAULT 0,  -- continuitygrade >= continuity_min
    roh_ok             INTEGER NOT NULL DEFAULT 0,  -- roh_est <= roh_ceiling
    reward_eligible    INTEGER NOT NULL DEFAULT 0,  -- all hard gates satisfied
    CHECK (window_end_ms > window_start_ms),
    CHECK (k_avg >= 0.0 AND k_avg <= 1.0),
    CHECK (e_avg >= 0.0 AND e_avg <= 1.0),
    CHECK (r_avg >= 0.0 AND r_avg <= 1.0),
    CHECK (vt_max >= 0.0),
    CHECK (roh_est >= 0.0 AND roh_est <= 1.0),
    CHECK (lane_ok IN (0,1)),
    CHECK (continuity_ok IN (0,1)),
    CHECK (roh_ok IN (0,1)),
    CHECK (reward_eligible IN (0,1))
);

CREATE INDEX IF NOT EXISTS idx_reward_exec_node_region
    ON reward_execution_window (nodeid, region, window_start_ms);

CREATE INDEX IF NOT EXISTS idx_reward_exec_eligibility
    ON reward_execution_window (reward_eligible, reward_family_id, host_profile_id);

CREATE TABLE IF NOT EXISTS reward_execution_plane (
    reward_exec_plane_id INTEGER PRIMARY KEY AUTOINCREMENT,
    reward_window_id     INTEGER NOT NULL REFERENCES reward_execution_window(reward_window_id) ON DELETE CASCADE,
    plane_id             INTEGER NOT NULL REFERENCES reward_plane(plane_id),
    r_start              REAL NOT NULL,     -- risk coordinate at window start
    r_end                REAL NOT NULL,     -- risk coordinate at window end
    delta_karma          REAL NOT NULL,     -- plane-specific karma uplift over window
    delta_mass_kg        REAL NOT NULL DEFAULT 0.0, -- cleanup / health mass-equivalent if applicable
    plane_ok             INTEGER NOT NULL DEFAULT 0, -- 1 if r_end <= r_start when mandatory
    CHECK (r_start >= 0.0 AND r_start <= 1.0),
    CHECK (r_end   >= 0.0 AND r_end   <= 1.0),
    CHECK (plane_ok IN (0,1))
);

CREATE INDEX IF NOT EXISTS idx_reward_exec_plane_window_plane
    ON reward_execution_plane (reward_window_id, plane_id);

------------------------------------------------------------
-- 5. Aggregated citizen rewards, decoupled from actuation
------------------------------------------------------------

CREATE TABLE IF NOT EXISTS reward_citizen_ledger (
    reward_entry_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    citizen_did         TEXT NOT NULL,
    reward_window_id    INTEGER NOT NULL REFERENCES reward_execution_window(reward_window_id),
    sink_id             INTEGER NOT NULL REFERENCES reward_token_sink(sink_id),
    token_symbol        TEXT NOT NULL,          -- e.g. 'ECOHEALTH', 'ECORESTORE'
    amount              REAL NOT NULL,          -- abstract token amount, on-chain mapping external
    created_ms          INTEGER NOT NULL,
    finalized_ms        INTEGER,                -- set when on-chain payment is confirmed
    status              TEXT NOT NULL DEFAULT 'PENDING', -- PENDING / FINALIZED / REJECTED
    reason_code         TEXT NOT NULL DEFAULT 'OK',
    UNIQUE (reward_window_id, citizen_did, token_symbol),
    CHECK (amount >= 0.0),
    CHECK (status IN ('PENDING','FINALIZED','REJECTED'))
);

CREATE INDEX IF NOT EXISTS idx_reward_citizen_status
    ON reward_citizen_ledger (citizen_did, status, created_ms);

------------------------------------------------------------
-- 6. MCP-facing read-only views for agent ecosystems
------------------------------------------------------------

-- View: v_reward_eligible_windows
-- Provides a compact surface for MCP tools to discover rewardable
-- MT6883 windows without touching internals of ecosafety KER tables.

DROP VIEW IF EXISTS v_reward_eligible_windows;

CREATE VIEW v_reward_eligible_windows AS
SELECT
    rew.reward_window_id,
    rew.nodeid,
    rew.region,
    h.profile_code,
    f.family_code,
    rew.window_start_ms,
    rew.window_end_ms,
    rew.lane,
    rew.k_avg,
    rew.e_avg,
    rew.r_avg,
    rew.vt_max,
    rew.roh_est
FROM reward_execution_window AS rew
JOIN mt6883_host_profile AS h
  ON rew.host_profile_id = h.host_profile_id
JOIN reward_window_family AS f
  ON rew.reward_family_id = f.reward_family_id
WHERE
    rew.reward_eligible = 1;

-- View: v_reward_plane_uplift
-- Plane-level uplift per eligible window, pre-normalized for MCP reward logic.

DROP VIEW IF EXISTS v_reward_plane_uplift;

CREATE VIEW v_reward_plane_uplift AS
SELECT
    rep.reward_window_id,
    rp.planecode,
    rp.nonoffsettable,
    rep.r_start,
    rep.r_end,
    rep.delta_karma,
    rep.delta_mass_kg,
    rep.plane_ok
FROM reward_execution_plane AS rep
JOIN reward_plane AS rp
  ON rep.plane_id = rp.plane_id;

-- View: v_reward_citizen_pending
-- Pending payouts with resolved primary sink address; MCP server can expose
-- this read-only to agents that propose on-chain distribution bundles.

DROP VIEW IF EXISTS v_reward_citizen_pending;

CREATE VIEW v_reward_citizen_pending AS
SELECT
    rcl.reward_entry_id,
    rcl.citizen_did,
    ts.chain_kind,
    ts.address,
    rcl.token_symbol,
    rcl.amount,
    rcl.created_ms,
    rcl.reason_code
FROM reward_citizen_ledger AS rcl
JOIN reward_token_sink AS ts
  ON rcl.sink_id = ts.sink_id
WHERE
    rcl.status = 'PENDING'
    AND ts.active = 1;
