-- File: sql/views/v_hex_stability_ker_dvt_carbon.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/views/v_hex_stability_ker_dvt_carbon.sql

PRAGMA foreign_keys = ON;

CREATE VIEW IF NOT EXISTS v_hex_stability_ker_dvt_carbon AS
SELECT
    h.hex_id                    AS hex_id,
    h.region_name               AS region_name,
    h.topology_band             AS topology_band,
    h.primary_plane             AS primary_plane,

    -- workload counts
    COUNT(w.workload_id)        AS workload_count,

    -- Lyapunov drift aggregates
    SUM(w.delta_v_t)            AS total_delta_v_t,
    AVG(w.delta_v_t)            AS avg_delta_v_t,
    MAX(w.delta_v_t)            AS max_delta_v_t,

    -- KER aggregates
    AVG(w.ker_k)                AS avg_ker_k,
    AVG(w.ker_e)                AS avg_ker_e,
    AVG(w.ker_r)                AS avg_ker_r,
    AVG(w.ker_s)                AS avg_ker_s,

    -- carbon intensity aggregates
    AVG(w.carbon_intensity_gco2_kwh) AS avg_carbon_intensity_gco2_kwh,
    SUM(CASE WHEN w.workload_window = 'GREEN_BAND' THEN 1 ELSE 0 END) AS count_green_band,
    SUM(CASE WHEN w.workload_window = 'NEUTRAL'    THEN 1 ELSE 0 END) AS count_neutral_band,
    SUM(CASE WHEN w.workload_window = 'RED_BAND'   THEN 1 ELSE 0 END) AS count_red_band,

    -- stability / policy violations
    SUM(CASE WHEN w.delta_v_t > 0.05 THEN 1 ELSE 0 END) AS violations_dvt_global,
    SUM(CASE WHEN w.ker_s <= 0.0 THEN 1 ELSE 0 END)     AS violations_ker_nonpositive,
    SUM(CASE
            WHEN w.fog_lane != 'RESEARCH'
                 AND w.delta_v_t > 0.10 * w.ker_s
            THEN 1 ELSE 0
        END) AS violations_joint_ker_dvt,

    -- carbon-aware governance violations
    SUM(CASE
            WHEN w.workload_window = 'RED_BAND'
                 AND (w.ker_s <= 0.3 OR w.ker_e < 0.8 OR w.ker_r > 0.3)
            THEN 1 ELSE 0
        END) AS violations_red_band_ker,
    SUM(CASE
            WHEN w.fog_lane = 'PROD' AND w.workload_window = 'RED_BAND'
            THEN 1 ELSE 0
        END) AS violations_prod_red_band
FROM phoenix_hex_registry AS h
LEFT JOIN cyboquatic_workload_telemetry AS w
    ON w.hex_id = h.hex_id
GROUP BY
    h.hex_id, h.region_name, h.topology_band, h.primary_plane
ORDER BY
    total_delta_v_t DESC,
    avg_carbon_intensity_gco2_kwh DESC;
