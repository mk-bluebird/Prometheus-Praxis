-- path: db/db_agent_safe_catalog.sql
-- role: AI-safe catalog schema for Prometheus-Praxis diagnostic tools and nodes.
--       Read-only to agents, keyed by corridor/node/lane, with KER/RoH/Lyapunov
--       diagnostics and always_improve scores, but no actuation or raw biosignals.

PRAGMA foreign_keys = ON;

-- Core catalog of AI-safe entries (tools, views, nodes).
CREATE TABLE IF NOT EXISTS agent_safe_catalog (
    catalog_id         TEXT PRIMARY KEY,            -- stable ID (e.g. ASC-000001)
    kind               TEXT NOT NULL,               -- 'TOOL' | 'NODE' | 'WINDOW'
    name               TEXT NOT NULL,               -- human-readable
    description        TEXT NOT NULL,               -- short description
    domain             TEXT NOT NULL,               -- ecosafety, cyboquatic, governance, etc.
    lane               TEXT NOT NULL,               -- RESEARCH | PILOT | PRODUCTION
    corridor_id        TEXT,                        -- corridor or city-object band
    smart_chain_id     TEXT,                        -- SMART chain governing this item
    ecosafety_required INTEGER NOT NULL DEFAULT 0,  -- mirrors ppx.function.meta.v1 flag
    superpower_adjacent INTEGER NOT NULL DEFAULT 0, -- 1 if near HeatWaterTree/Neurobiome/nanoswarm superpowers
    consent_stream_id  TEXT,                        -- optional ID for consented telemetry stream
    mcp_tool_name      TEXT,                        -- optional MCP tool name (for kind='TOOL')
    created_at         INTEGER NOT NULL,            -- unix epoch seconds
    updated_at         INTEGER NOT NULL             -- unix epoch seconds
);

CREATE INDEX IF NOT EXISTS idx_agent_safe_catalog_domain
    ON agent_safe_catalog (domain);

CREATE INDEX IF NOT EXISTS idx_agent_safe_catalog_lane
    ON agent_safe_catalog (lane);

CREATE INDEX IF NOT EXISTS idx_agent_safe_catalog_corridor
    ON agent_safe_catalog (corridor_id);

-- Diagnostics table keyed by catalog entry (NODE/WINDOW kinds).
-- Populated by Rust jobs, not by AI agents.
CREATE TABLE IF NOT EXISTS agent_safe_diagnostics (
    catalog_id         TEXT NOT NULL,
    -- Normalized KER outputs (0..1).
    k_score            REAL NOT NULL,
    e_score            REAL NOT NULL,
    r_score            REAL NOT NULL,
    -- Risk-of-harm and Lyapunov residuals (0..1).
    roh_scalar         REAL NOT NULL,
    v_current          REAL NOT NULL,
    v_next             REAL NOT NULL,
    lyap_delta         REAL NOT NULL,
    -- Precomputed always_improve scalar (0..1).
    always_improve     REAL NOT NULL,
    safe_to_promote    INTEGER NOT NULL, -- 0/1, matches AlwaysImproveScore.safe_to_promote
    -- Stability and margin classification for AI reasoning.
    stable_flag        INTEGER NOT NULL, -- 0/1: derived from Lyapunov + corridors
    margin_class       TEXT NOT NULL,    -- 'safe' | 'tight' | 'violated'
    -- Lane/corridor echo fields for quick filtering.
    lane               TEXT NOT NULL,
    corridor_id        TEXT,
    updated_at         INTEGER NOT NULL,
    PRIMARY KEY (catalog_id),
    FOREIGN KEY (catalog_id) REFERENCES agent_safe_catalog(catalog_id)
        ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_agent_safe_diag_lane
    ON agent_safe_diagnostics (lane);

CREATE INDEX IF NOT EXISTS idx_agent_safe_diag_corridor
    ON agent_safe_diagnostics (corridor_id);

CREATE INDEX IF NOT EXISTS idx_agent_safe_diag_safe_promote
    ON agent_safe_diagnostics (safe_to_promote);

-- Consent and neurorights guard table: which catalog entries are allowed
-- to surface which telemetry families to AI agents.
--
-- This table should be populated by the sovereign-consent engine; any row
-- with allowed = 0 means those fields must be hidden in AI-facing views.
CREATE TABLE IF NOT EXISTS agent_safe_consent_guard (
    catalog_id         TEXT NOT NULL,
    telemetry_family   TEXT NOT NULL,   -- 'KER' | 'ROH' | 'LYAP' | 'ECO_HEALTH_AGG' | ...
    allowed            INTEGER NOT NULL, -- 0/1
    reason             TEXT NOT NULL,   -- short consent/neurorights rationale
    updated_at         INTEGER NOT NULL,
    PRIMARY KEY (catalog_id, telemetry_family),
    FOREIGN KEY (catalog_id) REFERENCES agent_safe_catalog(catalog_id)
        ON DELETE CASCADE ON UPDATE CASCADE
);

-- AI-facing view:
-- - Only exposes entries where consent allows KER/ROH/LYAP.
-- - Never exposes actuation fields or raw biosignals.
-- - Filters out superpower-adjacent entries unless ecosafety_required != 0
--   (ensuring governance gates are in place).
CREATE VIEW IF NOT EXISTS v_agent_safe_catalog AS
SELECT
    c.catalog_id,
    c.kind,
    c.name,
    c.description,
    c.domain,
    c.lane,
    c.corridor_id,
    c.smart_chain_id,
    c.ecosafety_required,
    c.superpower_adjacent,
    d.k_score,
    d.e_score,
    d.r_score,
    d.roh_scalar,
    d.v_current,
    d.v_next,
    d.lyap_delta,
    d.always_improve,
    d.safe_to_promote,
    d.stable_flag,
    d.margin_class,
    d.updated_at AS diagnostics_updated_at
FROM agent_safe_catalog AS c
JOIN agent_safe_diagnostics AS d
    ON c.catalog_id = d.catalog_id
JOIN agent_safe_consent_guard AS g_ker
    ON g_ker.catalog_id = c.catalog_id
   AND g_ker.telemetry_family = 'KER'
   AND g_ker.allowed = 1
JOIN agent_safe_consent_guard AS g_roh
    ON g_roh.catalog_id = c.catalog_id
   AND g_roh.telemetry_family = 'ROH'
   AND g_roh.allowed = 1
JOIN agent_safe_consent_guard AS g_lyap
    ON g_lyap.catalog_id = c.catalog_id
   AND g_lyap.telemetry_family = 'LYAP'
   AND g_lyap.allowed = 1
WHERE
    -- Do not expose superpower-adjacent entries unless they are
    -- ecosafety-gated and thus require governance tokens to query.
    (c.superpower_adjacent = 0 OR c.ecosafety_required != 0);
