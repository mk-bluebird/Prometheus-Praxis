-- File: sql/views/v_ker_high_risk_tools.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/views/v_ker_high_risk_tools.sql

CREATE VIEW IF NOT EXISTS v_ker_high_risk_tools AS
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
    t.neuroflag,
    t.nonactuatingonly,
    t.citizen_ready,
    t.consent_level
FROM mcp_tool AS t
WHERE
    -- High eco-impact OR high risk-of-harm
    (t.ker_e >= 0.7 OR t.ker_r >= 0.5)
ORDER BY
    t.ker_r DESC,
    t.ker_e DESC;
