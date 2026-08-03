-- File: sql/views/v_prod_lane_candidates_modules.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/views/v_prod_lane_candidates_modules.sql

CREATE VIEW IF NOT EXISTS v_prod_lane_candidates_modules AS
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
    m.non_actuating,
    m.citizen_ready
FROM module_ker_profile AS m
WHERE
    -- Good candidates for PROD: not in PROD yet, strong KER
    m.lane_default != 'PROD'
    AND m.ker_s > 0.2
    AND m.ker_e >= 0.7
    AND m.ker_r <= 0.5
ORDER BY
    m.primary_plane,
    m.ker_s DESC;
