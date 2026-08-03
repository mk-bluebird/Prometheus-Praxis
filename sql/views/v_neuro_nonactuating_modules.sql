-- File: sql/views/v_neuro_nonactuating_modules.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/views/v_neuro_nonactuating_modules.sql

CREATE VIEW IF NOT EXISTS v_neuro_nonactuating_modules AS
SELECT
    m.module_id,
    m.repo_name,
    m.relpath,
    m.lane_default,
    m.primary_plane,
    m.module_role,
    m.neuro_flag,
    m.non_actuating,
    m.citizen_ready,
    m.ker_k,
    m.ker_e,
    m.ker_r,
    m.ker_s
FROM module_ker_profile AS m
WHERE
    m.neuro_flag = 1
    AND m.non_actuating = 1
ORDER BY
    m.lane_default,
    m.relpath;
