-- File: sql/cyboquatic_workload_carbon.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/cyboquatic_workload_carbon.sql

PRAGMA foreign_keys = ON;

ALTER TABLE cyboquatic_workload_telemetry
    ADD COLUMN carbon_intensity_gco2_kwh REAL NOT NULL DEFAULT 0.0;

ALTER TABLE cyboquatic_workload_telemetry
    ADD COLUMN workload_window TEXT NOT NULL DEFAULT 'NEUTRAL'
        CHECK (workload_window IN ('GREEN_BAND', 'NEUTRAL', 'RED_BAND'));

-- Governance trigger: restrict high-intensity workloads and production lanes.

DROP TRIGGER IF EXISTS trg_cybo_workload_carbon_corridor;

CREATE TRIGGER trg_cybo_workload_carbon_corridor
BEFORE INSERT ON cyboquatic_workload_telemetry
BEGIN
    -- If workload is in a RED_BAND window, require very strong KER and low risk.
    SELECT CASE
        WHEN NEW.workload_window = 'RED_BAND'
             AND (NEW.ker_s <= 0.3 OR NEW.ker_e < 0.8 OR NEW.ker_r > 0.3) THEN
            RAISE(ABORT, 'RED_BAND workloads require ker_s>0.3, ker_e>=0.8, ker_r<=0.3')
    END;

    -- For PROD lane workloads, discourage execution in RED_BAND at all.
    SELECT CASE
        WHEN NEW.fog_lane = 'PROD'
             AND NEW.workload_window = 'RED_BAND' THEN
            RAISE(ABORT, 'PROD workloads must not execute in RED_BAND carbon windows')
    END;
END;
