-- filename: ecorestorationshard/ecosafety_core_v2/sql/tda_residual_windows_30day.sql
-- destination: ecorestorationshard/ecosafety_core_v2/sql/tda_residual_windows_30day.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
--
-- Purpose:
--   Non-actuating SQLite shard to provide residual window data for
--   TDA (persistent homology) over 30 days.[4]

PRAGMA foreign_keys = ON;

-- Extract point cloud: (segment_id_index, day_index, vt_residual).
-- This is a helper view; the actual numeric embedding is done in C++.
CREATE VIEW IF NOT EXISTS v_tda_residual_30day AS
SELECT
    segment_id,
    yyyymmdd,
    vt_residual
FROM ker_residual_window
WHERE yyyymmdd BETWEEN '20260701' AND '20260730';
