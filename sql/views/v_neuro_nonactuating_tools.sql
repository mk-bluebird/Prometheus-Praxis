-- File: sql/views/v_neuro_nonactuating_tools.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/views/v_neuro_nonactuating_tools.sql

CREATE VIEW IF NOT EXISTS v_neuro_nonactuating_tools AS
SELECT
    t.toolid,
    t.toolname,
    t.roleband,
    t.lanedefault,
    t.primaryplane,
    t.neuroflag,
    t.nonactuatingonly,
    t.citizen_ready,
    t.consent_level,
    t.synapse_class,
    t.allows_actuation,
    t.ker_k,
    t.ker_e,
    t.ker_r,
    t.ker_s
FROM mcp_tool AS t
WHERE
    t.neuroflag = 1
    AND t.nonactuatingonly = 1
ORDER BY
    t.lanedefault,
    t.toolname;
