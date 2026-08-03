-- File: sql/mcp/mcp_hex_stability_tool.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/mcp/mcp_hex_stability_tool.sql

PRAGMA foreign_keys = ON;

-- Tool definition
INSERT INTO mcp_tool (
    toolname,
    summary,
    toolkind,
    resourcemode,
    roleband,
    primaryplane,
    lanedefault,
    nonactuatingonly,
    neuroflag,
    citizen_ready,
    ker_k,
    ker_e,
    ker_r,
    ker_s,
    synapse_class,
    allows_readonly,
    allows_actuation,
    consent_level
) VALUES (
    'hex_stability',
    'Return hex-level stability summary (ΔVt, KER aggregates, corridor violations) from v_hex_stability_ker_dvt.',
    'SQL_QUERY',
    'READONLY',
    'ENGINE',
    'HYDRAULICS',
    'RESEARCH',
    1,    -- nonactuatingonly
    0,    -- neuroflag
    1,    -- citizen_ready (safe for augmented citizens to view)
    0.9,  -- ker_k: high knowledge-factor (derived from governed telemetry)
    0.85, -- ker_e: strong eco-efficiency impact
    0.3,  -- ker_r: moderate risk (aggregates only, no actuation)
    0.9 * 0.85 - 0.3, -- ker_s
    'ANALYTIC_BRIDGE',
    1,    -- allows_readonly
    0,    -- allows_actuation
    'EXPLICIT'
);

-- Endpoint definition: STDIO MCP C++ server
INSERT INTO mcp_endpoint (
    toolid,
    endpoint_type,
    identifier,
    sql_view_name,
    rust_fn_path,
    script_entry,
    is_default
) VALUES (
    (SELECT toolid FROM mcp_tool WHERE toolname = 'hex_stability'),
    'CLI',
    'cpp/tools/mcp_ker_synapse_server',   -- the MCP server binary path
    'v_hex_stability_ker_dvt',
    NULL,
    NULL,
    1
);
