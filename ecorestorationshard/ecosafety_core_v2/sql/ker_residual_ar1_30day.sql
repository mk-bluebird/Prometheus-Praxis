-- filename: ecorestorationshard/ecosafety_core_v2/sql/ker_residual_ar1_30day.sql
-- destination: ecorestorationshard/ecosafety_core_v2/sql/ker_residual_ar1_30day.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
--
-- Purpose:
--   Provide a view of residual V_t over 30-day windows for AR(1)
--   and variance computation, for early-warning signals of KER band exit.[286][288][296]

PRAGMA foreign_keys = ON;

CREATE VIEW IF NOT EXISTS v_residual_30day AS
SELECT
    segment_id,
    yyyymmdd,
    vt_residual,
    lane
FROM ker_residual_window
WHERE yyyymmdd BETWEEN '20260701' AND '20260730';
