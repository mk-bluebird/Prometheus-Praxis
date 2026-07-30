-- filename: ecorestorationshard/ecosafety_core_v2/sql/pc_causal_data_view.sql
-- destination: ecorestorationshard/ecosafety_core_v2/sql/pc_causal_data_view.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
--
-- Purpose:
--   SQLite view to provide daily causal variables for PC algorithm
--   (Peter-Clark) conditional independence tests.[219]

PRAGMA foreign_keys = ON;

CREATE VIEW IF NOT EXISTS v_causal_data_daily AS
SELECT
    node_id,
    yyyymmdd,
    r_pfas,
    r_bod,
    r_tss,
    r_cec,
    energyreqJ,
    delta_Vt,
    K,
    E,
    R,
    actions_code
FROM causal_variables_daily;
