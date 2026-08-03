-- File: sql/views/v_hex_stability_ker_dvt.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/views/v_hex_stability_ker_dvt.sql

PRAGMA foreign_keys = ON;

-- Assumed registry table; adapt if already defined differently.
-- CREATE TABLE phoenix_hex_registry (
--     hex_id        TEXT PRIMARY KEY,
--     region_name   TEXT NOT NULL,
--     topology_band TEXT NOT NULL,
--     primary_plane TEXT NOT NULL
-- );

CREATE VIEW IF NOT EXISTS v_hex_stability_ker_dvt AS
SELECT
    h.hex_id                            AS hex_id,
    h.region_name                       AS region_name,
    h.topology_band                     AS topology_band,
    h.primary_plane                     AS primary_plane,

    -- aggregate Lyapunov drift and workload count
    COUNT(w.workload_id)                AS workload_count,
    SUM(w.delta_v_t)                    AS total_delta_v_t,
    AVG(w.delta_v_t)                    AS avg_delta_v_t,
    MAX(w.delta_v_t)                    AS max_delta_v_t,

    -- aggregate KER components
    AVG(w.ker_k)                        AS avg_ker_k,
    AVG(w.ker_e)                        AS avg_ker_e,
    AVG(w.ker_r)                        AS avg_ker_r,
    AVG(w.ker_s)                        AS avg_ker_s,

    -- stability / policy flags
    SUM(CASE WHEN w.delta_v_t > 0.05 THEN 1 ELSE 0 END) AS violations_dvt_global,
    SUM(CASE WHEN w.ker_s <= 0.0 THEN 1 ELSE 0 END)     AS violations_ker_nonpositive,
    SUM(CASE
            WHEN w.fog_lane != 'RESEARCH'
                 AND w.delta_v_t > 0.10 * w.ker_s
            THEN 1 ELSE 0
        END)                             AS violations_joint_ker_dvt
FROM phoenix_hex_registry AS h
LEFT JOIN cyboquatic_workload_telemetry AS w
    ON w.hex_id = h.hex_id
GROUP BY
    h.hex_id, h.region_name, h.topology_band, h.primary_plane
ORDER BY
    total_delta_v_t DESC,
    avg_delta_v_t DESC;
