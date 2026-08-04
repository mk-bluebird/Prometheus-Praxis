-- File: sql/bod_decay_time_varying_temperature.sql
-- BOD decay in a canal with time-varying temperature and plug-flow travel time τ.
--
-- Continuous-time model:
--   dBOD/dt = -k(T(t)) * BOD(t)
-- with temperature-dependent rate:
--   k(T) = k20 * θ^(T - 20),
-- where k20 is the rate at 20°C, θ is the temperature coefficient.
--
-- Solution for BOD at downstream sensor after plug-flow travel time τ:
--
--   BOD(τ) = BOD0 * exp( - ∫_0^τ k(T(s)) ds )
--          = BOD0 * exp( - ∫_0^τ k20 * θ^(T(s) - 20) ds ).
--
-- In discrete telemetry, we approximate the integral as a sum over time steps:
--
--   ∫_0^τ k(T(s)) ds ≈ Σ_{i} k(T_i) Δt_i,
--   where T_i is temperature at time segment i and Δt_i is duration.
--
-- We model this in SQL using window functions over telemetry timestamps.

PRAGMA foreign_keys = ON;

-- Telemetry table: temperature and BOD at upstream, per time sample.
CREATE TABLE IF NOT EXISTS canal_telemetry_upstream (
    sample_id INTEGER PRIMARY KEY AUTOINCREMENT,
    canal_id TEXT NOT NULL,
    timestamp_s REAL NOT NULL,
    temp_c REAL NOT NULL,  -- upstream temperature (°C)
    bod_mg_l REAL NOT NULL -- upstream BOD (mg/L)
);

CREATE INDEX IF NOT EXISTS idx_canal_upstream_time
    ON canal_telemetry_upstream(canal_id, timestamp_s);

-- Configuration table for decay parameters and plug-flow travel time per canal.
CREATE TABLE IF NOT EXISTS canal_decay_config (
    canal_id TEXT PRIMARY KEY,
    k20_per_s REAL NOT NULL,      -- base decay rate at 20°C (1/s)
    theta_coeff REAL NOT NULL,    -- temperature coefficient θ
    travel_time_s REAL NOT NULL   -- plug-flow travel time τ (s)
);

-- Downstream BOD calculation using window functions:
-- For a given canal and downstream observation time t_down, we consider the
-- upstream temperature history along the Lagrangian trajectory, i.e., the
-- interval [t_down - τ, t_down]. We approximate the integral ∫_0^τ k(T(s)) ds
-- by summing k(T_i) * Δt_i over samples in that interval, then compute:
--
--   BOD_downstream(t_down) = BOD_upstream(t_down - τ) *
--                            exp( - k20 * Σ θ^(T_i - 20) Δt_i )

-- Example query to compute downstream BOD at each downstream time stamp
-- using upstream telemetry only (assuming plug-flow and negligible dispersion):

-- 1. Create a helper view that computes k(T) per sample and Δt to the next sample.
CREATE VIEW IF NOT EXISTS canal_decay_kernel AS
SELECT
    u.canal_id,
    u.sample_id,
    u.timestamp_s,
    u.temp_c,
    u.bod_mg_l,
    cfg.k20_per_s,
    cfg.theta_coeff,
    cfg.travel_time_s,
    -- temperature-dependent rate k(T) = k20 * θ^(T - 20)
    (cfg.k20_per_s * pow(cfg.theta_coeff, (u.temp_c - 20.0))) AS k_t_per_s,
    -- Δt between this sample and the next sample for the same canal
    (LEAD(u.timestamp_s) OVER (PARTITION BY u.canal_id ORDER BY u.timestamp_s)
     - u.timestamp_s) AS delta_t_s
FROM canal_telemetry_upstream u
JOIN canal_decay_config cfg ON cfg.canal_id = u.canal_id;

-- 2. For each downstream observation time t_down, approximate the integral over [t_down - τ, t_down]
--    by summing k_t_per_s * delta_t_s over samples in that window, and compute BOD_downstream.
--    Here we assume downstream observation times align with upstream samples for simplicity.

-- Example downstream BOD computation view:
CREATE VIEW IF NOT EXISTS canal_bod_downstream AS
WITH upstream_lagrangian AS (
    SELECT
        cdk.canal_id,
        cdk.sample_id,
        cdk.timestamp_s AS t_up,
        cdk.temp_c,
        cdk.bod_mg_l AS bod_up_mg_l,
        cdk.k20_per_s,
        cdk.theta_coeff,
        cdk.travel_time_s,
        cdk.k_t_per_s,
        COALESCE(cdk.delta_t_s, 0.0) AS delta_t_s
    FROM canal_decay_kernel cdk
)
SELECT
    u.canal_id,
    u.sample_id AS downstream_sample_id,
    (u.timestamp_s + u.travel_time_s) AS downstream_timestamp_s,
    -- Integral approximation: sum k_t_per_s * delta_t_s over [t_up, t_up + τ].
    -- We use a window over upstream samples whose timestamps lie in this interval.
    u.bod_up_mg_l *
    EXP(
        - u.k20_per_s *
        (
            SELECT SUM(k_t_per_s * delta_t_s)
            FROM upstream_lagrangian ul
            WHERE ul.canal_id = u.canal_id
              AND ul.timestamp_s >= u.timestamp_s
              AND ul.timestamp_s <= u.timestamp_s + u.travel_time_s
        )
    ) AS bod_downstream_mg_l
FROM upstream_lagrangian u;

-- Usage:
-- SELECT * FROM canal_bod_downstream WHERE canal_id = 'canal-01' ORDER BY downstream_timestamp_s;
