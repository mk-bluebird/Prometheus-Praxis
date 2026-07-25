-- filename: db/db_hex_thermal_ema_hotspots.sql

WITH ema AS (
    SELECT
        hex_id,
        date,
        -- Approximate 30-day EMA using windowed weighted average
        SUM(r_thermal * weight) OVER (
            PARTITION BY hex_id
            ORDER BY date
            ROWS BETWEEN 29 PRECEDING AND CURRENT ROW
        ) /
        SUM(weight) OVER (
            PARTITION BY hex_id
            ORDER BY date
            ROWS BETWEEN 29 PRECEDING AND CURRENT ROW
        ) AS ema_30
    FROM (
        SELECT
            hex_id,
            date,
            r_thermal,
            -- Example weight: exponential decay by days difference; adjust as needed.
            EXP(-0.1 * (julianday('now') - julianday(date))) AS weight
        FROM hex_thermal_daily
    )
),
ema_flags AS (
    SELECT
        hex_id,
        date,
        ema_30,
        CASE WHEN ema_30 > 0.7 THEN 1 ELSE 0 END AS hotspot_flag
    FROM ema
),
runs AS (
    SELECT
        hex_id,
        date,
        ema_30,
        hotspot_flag,
        -- Count consecutive days of hotspot_flag = 1
        SUM(hotspot_flag) OVER (
            PARTITION BY hex_id
            ORDER BY date
            ROWS BETWEEN 6 PRECEDING AND CURRENT ROW
        ) AS hotspot_window_sum
    FROM ema_flags
)
SELECT
    hex_id,
    date,
    ema_30,
    hotspot_flag,
    hotspot_window_sum,
    CASE WHEN hotspot_window_sum >= 7 AND hotspot_flag = 1 THEN 1 ELSE 0 END AS persistent_hotspot
FROM runs;
