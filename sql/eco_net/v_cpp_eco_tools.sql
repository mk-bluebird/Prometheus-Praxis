-- File: sql/eco_net/v_cpp_eco_tools.sql

-- SQL MCP tool discovery views for eco C++ tools.
-- This view joins mcp_tool, mcp_file, and mcp_endpoint to expose
-- all eco-restoration C++ tools whose endpoint_type is CLI or CPP_FN,
-- making discovery trivial for AI agents.[78]

CREATE VIEW IF NOT EXISTS v_cpp_eco_tools AS
SELECT
    mt.toolid,
    mt.toolname,
    mt.summary,
    mt.toolkind,
    mt.resourcemode,
    mt.lanedefault,
    mt.planebands,
    mt.ker_hint,
    me.endpoint_type,
    me.identifier,
    me.sql_view_name,
    me.rust_fn_path,
    me.script_entry,
    mf.relpath,
    mf.filename,
    mr.reponame,
    mr.githubslug
FROM mcp_tool mt
JOIN mcp_file mf
    ON mt.fileid = mf.fileid
JOIN mcp_repo mr
    ON mt.repoid = mr.repoid
JOIN mcp_endpoint me
    ON me.toolid = mt.toolid
WHERE mr.reponame = 'eco_restoration_shard'
  AND mt.active = 1
  AND (me.endpoint_type = 'CLI' OR me.endpoint_type = 'CPP_FN')
  AND mf.filekind IN ('CPP', 'OTHER');

-- Optional helper view focusing on eco-restoration planes.
CREATE VIEW IF NOT EXISTS v_cpp_eco_tools_plane AS
SELECT
    toolname,
    summary,
    planebands,
    endpoint_type,
    identifier,
    relpath,
    filename
FROM v_cpp_eco_tools;
