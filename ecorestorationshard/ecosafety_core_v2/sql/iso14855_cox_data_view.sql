-- filename: ecorestorationshard/ecosafety_core_v2/sql/iso14855_cox_data_view.sql
-- destination: ecorestorationshard/ecosafety_core_v2/sql/iso14855_cox_data_view.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
--
-- Purpose:
--   SQLite view exposing ISO 14855 test data for Cox proportional
--   hazards modeling:
--     - survival time until biodegradation threshold.
--     - event indicator.
--     - time-varying covariates r_T and r_PFAS.[211][218]

PRAGMA foreign_keys = ON;

CREATE VIEW IF NOT EXISTS v_iso14855_cox_data AS
SELECT
    test_id,
    time_days,
    event_binary,    -- 1 if biodegradation threshold reached, 0 otherwise
    r_T,
    r_pfas
FROM iso14855_biodegradation_telemetry;
