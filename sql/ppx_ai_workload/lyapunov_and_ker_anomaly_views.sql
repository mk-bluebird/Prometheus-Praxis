-- File: sql/ppx_ai_workload/lyapunov_and_ker_anomaly_views.sql
CREATE TABLE IF NOT EXISTS ppx_lyapunov_snapshot (
    source_id TEXT NOT NULL,
    observed_utc TEXT NOT NULL,
    vt REAL NOT NULL CHECK(vt >= 0.0),
    PRIMARY KEY(source_id, observed_utc)
) STRICT;

CREATE VIEW IF NOT EXISTS v_ppx_second_order_residual AS
WITH ordered AS (
    SELECT
        source_id,
        observed_utc,
        vt,
        LAG(vt, 1) OVER (
            PARTITION BY source_id ORDER BY observed_utc
        ) AS vt_previous,
        LEAD(vt, 1) OVER (
            PARTITION BY source_id ORDER BY observed_utc
        ) AS vt_next
    FROM ppx_lyapunov_snapshot
)
SELECT
    source_id,
    observed_utc,
    vt_previous,
    vt AS vt_current,
    vt_next,
    vt_next - vt AS delta_vt_forward,
    vt_next - 2.0 * vt + vt_previous AS delta2_vt,
    CASE
        WHEN vt_previous IS NULL OR vt_next IS NULL THEN 0
        WHEN vt_next - 2.0 * vt + vt_previous > 0.0 THEN 1
        ELSE 0
    END AS oscillatory_instability_flag
FROM ordered;

CREATE VIEW IF NOT EXISTS v_ppx_ker_knowledge_anomaly AS
WITH time_indexed AS (
    SELECT
        source_id,
        observed_utc,
        k_knowledge,
        CAST(strftime('%s', observed_utc) AS INTEGER) AS observed_epoch_s
    FROM ppx_ker_fog_canal_shard
),
statistics AS (
    SELECT
        source_id,
        observed_utc,
        k_knowledge,
        COUNT(k_knowledge) OVER seven_days AS samples_in_window,
        AVG(k_knowledge) OVER seven_days AS moving_mean,
        AVG(k_knowledge * k_knowledge) OVER seven_days AS moving_mean_square
    FROM time_indexed
    WINDOW seven_days AS (
        PARTITION BY source_id
        ORDER BY observed_epoch_s
        RANGE BETWEEN 604800 PRECEDING AND 1 PRECEDING
    )
)
SELECT
    source_id,
    observed_utc,
    k_knowledge,
    samples_in_window,
    moving_mean,
    CASE
        WHEN moving_mean IS NULL THEN NULL
        ELSE MAX(0.0, moving_mean_square - moving_mean * moving_mean)
    END AS moving_variance,
    CASE
        WHEN samples_in_window < 2 THEN 0
        WHEN k_knowledge >= moving_mean THEN 0
        WHEN (moving_mean - k_knowledge) * (moving_mean - k_knowledge) >
             4.0 * MAX(0.0, moving_mean_square - moving_mean * moving_mean)
        THEN 1
        ELSE 0
    END AS knowledge_drop_two_sigma_flag
FROM statistics;
