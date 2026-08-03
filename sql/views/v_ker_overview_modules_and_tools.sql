-- File: sql/views/v_ker_overview_modules_and_tools.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/views/v_ker_overview_modules_and_tools.sql

CREATE VIEW IF NOT EXISTS v_ker_overview_modules_and_tools AS
SELECT
    'MODULE' AS kind,
    m.repo_name AS repo_or_roleband,
    m.relpath   AS name_or_relpath,
    m.lane_default,
    m.primary_plane,
    m.module_role AS role,
    m.ker_k,
    m.ker_e,
    m.ker_r,
    m.ker_s,
    m.neuro_flag,
    m.non_actuating AS non_actuating,
    m.citizen_ready AS citizen_ready
FROM module_ker_profile AS m

UNION ALL

SELECT
    'TOOL' AS kind,
    t.roleband AS repo_or_roleband,
    t.toolname AS name_or_relpath,
    t.lanedefault,
    t.primaryplane,
    t.synapse_class AS role,
    t.ker_k,
    t.ker_e,
    t.ker_r,
    t.ker_s,
    t.neuroflag,
    t.nonactuatingonly AS non_actuating,
    t.citizen_ready
FROM mcp_tool AS t;
