-- File: sql/pfas_lyapunov_residual.sql
PRAGMA foreign_keys = ON;

-- Telemetry table for PFAS concentration per canal segment and time.
CREATE TABLE IF NOT EXISTS pfas_corridor_telemetry (
    sample_id INTEGER PRIMARY KEY AUTOINCREMENT,
    canal_segment TEXT NOT NULL,
    timestamp_s REAL NOT NULL,
    pfas_ug_l REAL NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_pfas_segment_time
    ON pfas_corridor_telemetry(canal_segment, timestamp_s);

-- Lyapunov residual recurrence:
-- We define a local separation δ(t) as the difference in PFAS concentration
-- between successive samples (or a small perturbation proxy). For a given
-- canal_segment, approximate the Lyapunov residual over time as:
--
--   lyap_res(t_{n+1}) = lyap_res(t_n) + dt_n * (log|δ_{n+1}/δ_n|) / (t_{n+1} - t0),
--
-- where:
--   dt_n = t_{n+1} - t_n,
--   t0 is the initial time in the window,
--   δ_n = pfas_ug_l(t_n) - pfas_ug_l(t_{n-1}) (or small separation proxy),
--   lyap_res(t0) = 0.
--
-- In SQL, we approximate this over a sliding window using window functions.

-- Helper view: compute δ_n and dt_n per canal_segment.
CREATE VIEW IF NOT EXISTS pfas_corridor_deltas AS
SELECT
    canal_segment,
    sample_id,
    timestamp_s,
    pfas_ug_l,
    -- Previous PFAS and timestamp for this segment
    LAG(pfas_ug_l) OVER (PARTITION BY canal_segment ORDER BY timestamp_s) AS pfas_prev,
    LAG(timestamp_s) OVER (PARTITION BY canal_segment ORDER BY timestamp_s) AS ts_prev
FROM pfas_corridor_telemetry;

-- View: Lyapunov residual per sample, relative to window start t0.
-- We choose t0 as the earliest timestamp in the window for each canal_segment.
CREATE VIEW IF NOT EXISTS pfas_lyap_residual AS
WITH deltas AS (
    SELECT
        canal_segment,
        sample_id,
        timestamp_s,
        pfas_ug_l,
        pfas_prev,
        ts_prev,
        -- δ_n = pfas_n - pfas_{n-1}
        (pfas_ug_l - pfas_prev) AS delta_n,
        -- dt_n = t_n - t_{n-1}
        (timestamp_s - ts_prev) AS dt_n
    FROM pfas_corridor_deltas
),
window_start AS (
    SELECT
        canal_segment,
        MIN(timestamp_s) AS t0
    FROM pfas_corridor_telemetry
    GROUP BY canal_segment
)
SELECT
    d.canal_segment,
    d.sample_id,
    d.timestamp_s,
    d.pfas_ug_l,
    d.delta_n,
    d.dt_n,
    ws.t0,
    -- Lyapunov residual recurrence approximated via cumulative sum:
    -- lyap_res(t_n) ≈ Σ_{k=1..n-1} dt_k * (log|δ_k/δ_{k-1}|) / (t_k - t0).
    -- We implement this as a windowed SUM, handling NULLs for first samples.
    SUM(
        CASE
            WHEN d.delta_n IS NOT NULL
                 AND LAG(d.delta_n) OVER (PARTITION BY d.canal_segment ORDER BY d.timestamp_s) IS NOT NULL
                 AND d.dt_n IS NOT NULL
            THEN d.dt_n *
                 (LOG(
                     ABS(d.delta_n) /
                     ABS(LAG(d.delta_n) OVER (PARTITION BY d.canal_segment ORDER BY d.timestamp_s))
                 ) / (d.timestamp_s - ws.t0))
            ELSE 0.0
        END
    ) OVER (PARTITION BY d.canal_segment ORDER BY d.timestamp_s
            ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS lyap_res
FROM deltas d
JOIN window_start ws ON ws.canal_segment = d.canal_segment;

-- This view yields (canal_segment, timestamp_s, lyap_res) per sample, approximating
-- the Lyapunov residual over time for each corridor segment.

-- Integration into a Lua-fed view:
-- Define a simplified view that the Lua corridor analyser can query:
CREATE VIEW IF NOT EXISTS pfas_lyap_corridor_feed AS
SELECT
    canal_segment,
    timestamp_s,
    pfas_ug_l,
    lyap_res
FROM pfas_lyap_residual
ORDER BY canal_segment, timestamp_s;
