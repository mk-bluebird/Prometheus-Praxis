-- filename: db_mcp_resource_index.sql
-- destination: ecorestorationshard/sql/spine/db_mcp_resource_index.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. MCP-capable repo registry (EcoNet constellation scope)
--    Each row is a logical repo that can expose MCP tools/resources.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS mcp_repo (
    repoid        INTEGER PRIMARY KEY AUTOINCREMENT,
    -- Stable logical name, e.g. 'EcoNet', 'eco_restoration_shard', 'Data_Lake'.
    reponame      TEXT NOT NULL UNIQUE,
    -- GitHub or remote slug, e.g. 'mk-bluebird/eco_restoration_shard'.
    githubslug    TEXT NOT NULL,
    -- Role band from EcoNet spine: SPINE, RESEARCH, ENGINE, MATERIAL, GOV, APP.
    roleband      TEXT NOT NULL CHECK (roleband IN ('SPINE','RESEARCH','ENGINE','MATERIAL','GOV','APP')),
    -- Primary ecological plane emphasized by this repo, e.g. 'HYDRO','CARBON','BIODIVERSITY','MATERIAL','NEURO'.
    primaryplane  TEXT NOT NULL,
    -- Default lane for MCP tools: RESEARCH, EXPPROD, PROD (ENGINE lanes must be explicit elsewhere).
    lanedefault   TEXT NOT NULL CHECK (lanedefault IN ('RESEARCH','EXPPROD','PROD')),
    -- True if this repo is non‑actuating only (analysis/education/visualization).
    nonactuatingonly INTEGER NOT NULL DEFAULT 1 CHECK (nonactuatingonly IN (0,1)),
    -- Owner DID, consistent with EcoNet DID registry.
    didowner      TEXT NOT NULL,
    -- Human readable description for agents.
    description   TEXT NOT NULL,
    createdutc    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updatedutc    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TRIGGER IF NOT EXISTS trg_mcp_repo_updated
AFTER UPDATE ON mcp_repo
BEGIN
    UPDATE mcp_repo
    SET updatedutc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE repoid = NEW.repoid;
END;

----------------------------------------------------------------------
-- 2. File index for MCP resources
--    Every file that can be exposed to MCP as a tool/resource.
--    This table is the primary discovery surface for AI agents.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS mcp_file (
    fileid        INTEGER PRIMARY KEY AUTOINCREMENT,
    repoid        INTEGER NOT NULL REFERENCES mcp_repo(repoid) ON DELETE CASCADE,
    -- Relative path within repo root, e.g. 'sql/spine/dbdefinitionregistry.sql'.
    relpath       TEXT NOT NULL,
    -- Logical filename (basename) for convenience.
    filename      TEXT NOT NULL,
    -- File kind: SQL, RUST, ALN, CSV, DOC, LUA, KOTLIN, ANDROID, JSON, OTHER.
    filekind      TEXT NOT NULL CHECK (
        filekind IN ('SQL','RUST','ALN','CSV','DOC','LUA','KOTLIN','ANDROID','JSON','OTHER')
    ),
    -- High‑level role: SCHEMA, INDEX, MCP_TOOL, CONFIG, DOC, VIEW, DATASET.
    filerole      TEXT NOT NULL CHECK (
        filerole IN ('SCHEMA','INDEX','MCP_TOOL','CONFIG','DOC','VIEW','DATASET')
    ),
    -- Plane bands touched by this file, e.g. 'HYDRO,CARBON,BIODIVERSITY', 'NEURO', 'MATERIAL'.
    planebands    TEXT NOT NULL,
    -- Optional crate name (for Rust), ALN schema id, or SQLite schema tag.
    logicalname   TEXT,
    -- True if this file is currently active and should be surfaced to MCP.
    active        INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    -- Optional DID of maintainer or signer for this file.
    maintainerdid TEXT,
    createdutc    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updatedutc    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    UNIQUE (repoid, relpath)
);

CREATE TRIGGER IF NOT EXISTS trg_mcp_file_updated
AFTER UPDATE ON mcp_file
BEGIN
    UPDATE mcp_file
    SET updatedutc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE fileid = NEW.fileid;
END;

CREATE INDEX IF NOT EXISTS idx_mcp_file_repo_active
    ON mcp_file (repoid, active);

CREATE INDEX IF NOT EXISTS idx_mcp_file_kind_role
    ON mcp_file (filekind, filerole);

----------------------------------------------------------------------
-- 3. MCP tool registry
--    Logical tools that MCP servers can expose, bound to files and repos.
--    Examples: 'eco_spine_ker_report', 'eco_pricing_rank', 'neuro_citizen_guard_eval'.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS mcp_tool (
    toolid        INTEGER PRIMARY KEY AUTOINCREMENT,
    repoid        INTEGER NOT NULL REFERENCES mcp_repo(repoid) ON DELETE CASCADE,
    fileid        INTEGER NOT NULL REFERENCES mcp_file(fileid) ON DELETE CASCADE,
    -- Tool name as seen by MCP clients, must be unique constellation‑wide.
    toolname      TEXT NOT NULL UNIQUE,
    -- Short human readable summary.
    summary       TEXT NOT NULL,
    -- MCP resource kind or transport: 'COMMAND', 'FILE', 'HTTP', 'SQL_QUERY', 'RUST_FN'.
    toolkind      TEXT NOT NULL CHECK (
        toolkind IN ('COMMAND','FILE','HTTP','SQL_QUERY','RUST_FN')
    ),
    -- Resource mode: READONLY or MUTATING (mutating must be explicitly governed).
    resourcemode  TEXT NOT NULL CHECK (resourcemode IN ('READONLY','MUTATING')),
    -- Default lane for this tool: RESEARCH, EXPPROD, PROD (should align with repo.lanedefault).
    lanedefault   TEXT NOT NULL CHECK (lanedefault IN ('RESEARCH','EXPPROD','PROD')),
    -- Planes this tool reasons about, e.g. 'KER', 'ECOWEALTH', 'NEURO', 'MATERIAL'.
    planebands    TEXT NOT NULL,
    -- True if this tool is safe for use by augmented citizens in their personal environment.
    citizen_ready INTEGER NOT NULL DEFAULT 0 CHECK (citizen_ready IN (0,1)),
    -- True if this tool touches any neuro/augmentation lane semantics.
    neuroflag     INTEGER NOT NULL DEFAULT 0 CHECK (neuroflag IN (0,1)),
    -- Optional ALN schema id describing the input payload.
    input_schema  TEXT,
    -- Optional ALN schema id describing the output payload.
    output_schema TEXT,
    -- Optional Bostrom DID for the steward responsible for this tool.
    stewarddid    TEXT,
    -- Optional K/E/R band hints (0–1) encoded as 'K=0.9,E=0.9,R=0.1'.
    ker_hint      TEXT,
    active        INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    createdutc    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updatedutc    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TRIGGER IF NOT EXISTS trg_mcp_tool_updated
AFTER UPDATE ON mcp_tool
BEGIN
    UPDATE mcp_tool
    SET updatedutc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE toolid = NEW.toolid;
END;

CREATE INDEX IF NOT EXISTS idx_mcp_tool_repo
    ON mcp_tool (repoid, active);

CREATE INDEX IF NOT EXISTS idx_mcp_tool_neuro_lane
    ON mcp_tool (neuroflag, lanedefault, citizen_ready);

----------------------------------------------------------------------
-- 4. Dedicated neuro / augmented-citizen capability track
--    This table specializes mcp_tool entries for neuro planes.
--    It ensures explicit governance, RoH ceilings, and continuity anchors.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS neuro_capability (
    neurocapid        INTEGER PRIMARY KEY AUTOINCREMENT,
    toolid            INTEGER NOT NULL REFERENCES mcp_tool(toolid) ON DELETE CASCADE,
    -- Neuro capability type: 'COGNITIVE_GUARD', 'EDU_PROMPT', 'BIOFEEDBACK_ANALYSIS', 'NEURO_METRIC_REPORT'.
    capability_type   TEXT NOT NULL CHECK (
        capability_type IN (
            'COGNITIVE_GUARD',
            'EDU_PROMPT',
            'BIOFEEDBACK_ANALYSIS',
            'NEURO_METRIC_REPORT'
        )
    ),
    -- RoH ceiling for this capability, e.g. 0.3.
    roh_ceiling       REAL NOT NULL CHECK (roh_ceiling BETWEEN 0.0 AND 1.0),
    -- K/E/R band hints specialized for neuro plane, e.g. 'K=0.95,E=0.80,R=0.10'.
    ker_neuro_hint    TEXT NOT NULL,
    -- Link into continuity anchor registry (psychological continuity constraints).
    continuityanchorid TEXT,
    -- Policy code for fear/pain boundaries, aligns with fearpainboundarypolicy.policycode.
    fearpain_policycode TEXT,
    -- True if this tool is allowed for MT6883/healthcare corridors; false otherwise.
    healthcare_lane   INTEGER NOT NULL DEFAULT 0 CHECK (healthcare_lane IN (0,1)),
    -- True if the capability is strictly diagnostic (no actuation, no prescriptions).
    diagnostic_only   INTEGER NOT NULL DEFAULT 1 CHECK (diagnostic_only IN (0,1)),
    -- Optional description of allowed user identities or cohorts.
    cohort_scope      TEXT,
    createdutc        TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updatedutc        TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    UNIQUE (toolid)
);

CREATE TRIGGER IF NOT EXISTS trg_neuro_capability_updated
AFTER UPDATE ON neuro_capability
BEGIN
    UPDATE neuro_capability
    SET updatedutc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE neurocapid = NEW.neurocapid;
END;

CREATE INDEX IF NOT EXISTS idx_neuro_capability_type
    ON neuro_capability (capability_type, healthcare_lane, diagnostic_only);

----------------------------------------------------------------------
-- 5. MCP resource endpoint bindings
--    Connect tools to concrete MCP endpoints (CLI commands, SQL queries, etc.)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS mcp_endpoint (
    endpointid    INTEGER PRIMARY KEY AUTOINCREMENT,
    toolid        INTEGER NOT NULL REFERENCES mcp_tool(toolid) ON DELETE CASCADE,
    -- Endpoint type: 'CLI', 'SQL', 'RUST_FN', 'LUA', 'KOTLIN', 'ANDROID_VIEW'.
    endpoint_type TEXT NOT NULL CHECK (
        endpoint_type IN ('CLI','SQL','RUST_FN','LUA','KOTLIN','ANDROID_VIEW')
    ),
    -- Endpoint identifier; for CLI it's the command name, for SQL a view name, etc.
    identifier    TEXT NOT NULL,
    -- Optional SQL snippet name or view backing this tool, e.g. 'vshard_stewardecowealth'.
    sql_view_name TEXT,
    -- Optional Rust function path, e.g. 'kerresidual::compute_residual'.
    rust_fn_path  TEXT,
    -- Optional Lua/Kotlin entrypoint name.
    script_entry  TEXT,
    -- True if this endpoint is the default binding for the tool.
    is_default    INTEGER NOT NULL DEFAULT 1 CHECK (is_default IN (0,1)),
    createdutc    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updatedutc    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    UNIQUE (toolid, endpoint_type, identifier)
);

CREATE TRIGGER IF NOT EXISTS trg_mcp_endpoint_updated
AFTER UPDATE ON mcp_endpoint
BEGIN
    UPDATE mcp_endpoint
    SET updatedutc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE endpointid = NEW.endpointid;
END;

CREATE INDEX IF NOT EXISTS idx_mcp_endpoint_tool
    ON mcp_endpoint (toolid, endpoint_type, is_default);

----------------------------------------------------------------------
-- 6. Views for AI-chat / MCP agents
--    These simplify discovery of MCP tools and neuro capabilities.
----------------------------------------------------------------------

-- View: all active MCP tools with repo and file metadata.
CREATE VIEW IF NOT EXISTS v_mcp_tools AS
SELECT
    r.reponame,
    r.githubslug,
    r.roleband,
    r.primaryplane,
    r.lanedefault AS repo_lanedefault,
    r.nonactuatingonly,
    f.fileid,
    f.relpath,
    f.filekind,
    f.filerole,
    f.planebands AS file_planebands,
    t.toolid,
    t.toolname,
    t.summary,
    t.toolkind,
    t.resourcemode,
    t.lanedefault AS tool_lanedefault,
    t.planebands AS tool_planebands,
    t.citizen_ready,
    t.neuroflag,
    t.input_schema,
    t.output_schema,
    t.stewarddid,
    t.ker_hint
FROM mcp_tool AS t
JOIN mcp_repo AS r ON r.repoid = t.repoid
JOIN mcp_file AS f ON f.fileid = t.fileid
WHERE t.active = 1
  AND f.active = 1;

-- View: dedicated neuro / augmented-citizen tools, joined to their capability rows.
CREATE VIEW IF NOT EXISTS v_neuro_capabilities AS
SELECT
    v.reponame,
    v.githubslug,
    v.roleband,
    v.primaryplane,
    v.toolname,
    v.summary,
    v.toolkind,
    v.resourcemode,
    v.tool_lanedefault,
    v.tool_planebands,
    v.citizen_ready,
    n.capability_type,
    n.roh_ceiling,
    n.ker_neuro_hint,
    n.continuityanchorid,
    n.fearpain_policycode,
    n.healthcare_lane,
    n.diagnostic_only,
    n.cohort_scope
FROM v_mcp_tools AS v
JOIN neuro_capability AS n ON n.toolid = v.toolid
WHERE v.neuroflag = 1;
