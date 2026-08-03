-- File: sql/views/v_prod_lane_violations_tools.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/views/v_prod_lane_violations_tools.sql

CREATE VIEW IF NOT EXISTS v_prod_lane_violations_tools AS
SELECT
    t.toolid,
    t.toolname,
    t.roleband,
    t.lanedefault,
    t.primaryplane,
    t.ker_k,
    t.ker_e,
    t.ker_r,
    t.ker_s,
    t.citizen_ready,
    t.nonactuatingonly,
    t.synapse_class
FROM mcp_tool AS t
WHERE
    t.lanedefault = 'PROD'
    AND (
        t.ker_s <= 0.2 OR
        t.ker_e < 0.7 OR
        t.ker_r > 0.5 OR
        t.citizen_ready = 0 OR
        t.synapse_class = 'ACTUATION_GATE'
    )
ORDER BY
    t.primaryplane,
    t.toolname;
