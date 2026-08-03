-- File: sql/views/v_ker_high_risk_modules.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/views/v_ker_high_risk_modules.sql

CREATE VIEW IF NOT EXISTS v_ker_high_risk_modules AS
SELECT
    m.module_id,
    m.repo_name,
    m.relpath,
    m.lane_default,
    m.primary_plane,
    m.module_role,
    m.ker_k,
    m.ker_e,
    m.ker_r,
    m.ker_s,
    m.neuro_flag,
    m.non_actuating,
    m.citizen_ready
FROM module_ker_profile AS m
WHERE
    -- High-risk threshold: ker_r >= 0.7 or ker_s <= 0 (poor corridor)
    (m.ker_r >= 0.7 OR m.ker_s <= 0.0)
ORDER BY
    m.ker_r DESC,
    m.ker_s ASC;
