-- File: sql/cyboquatic_workload_telemetry.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/cyboquatic_workload_telemetry.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS cyboquatic_workload_telemetry (
    workload_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    node_id            TEXT NOT NULL,
    timestamputc       TEXT NOT NULL,
    flow_rate_m3s      REAL NOT NULL CHECK (flow_rate_m3s >= 0.0),
    head_loss_m        REAL NOT NULL CHECK (head_loss_m >= 0.0),
    pump_power_kw      REAL NOT NULL CHECK (pump_power_kw >= 0.0),
    lift_height_m      REAL NOT NULL CHECK (lift_height_m >= 0.0),
    water_density_kgm3 REAL NOT NULL CHECK (water_density_kgm3 > 0.0),
    gravity_ms2        REAL NOT NULL CHECK (gravity_ms2 > 0.0),
    eco_efficiency     REAL NOT NULL CHECK (eco_efficiency >= 0.0 AND eco_efficiency <= 1.0),
    energyreq_j        REAL NOT NULL CHECK (energyreq_j >= 0.0),
    eco_energy_j       REAL NOT NULL CHECK (eco_energy_j >= 0.0),
    delta_v_t          REAL NOT NULL CHECK (delta_v_t >= 0.0),
    ker_k              REAL NOT NULL CHECK (ker_k >= 0.0 AND ker_k <= 1.0),
    ker_e              REAL NOT NULL CHECK (ker_e >= 0.0 AND ker_e <= 1.0),
    ker_r              REAL NOT NULL CHECK (ker_r >= 0.0 AND ker_r <= 1.0),
    canal_plane        TEXT NOT NULL CHECK (canal_plane IN ('HYDRAULICS', 'ENERGY', 'TOPOLOGY', 'BIODIVERSITY')),
    fog_lane           TEXT NOT NULL CHECK (fog_lane IN ('RESEARCH', 'EXPPROD', 'PROD')),
    canal_node_class   TEXT NOT NULL CHECK (canal_node_class IN ('INTAKE', 'LIFT', 'DISTRIBUTION', 'DRAIN')),
    phoenix_hex        TEXT NOT NULL,
    subtask_id         TEXT NOT NULL,
    domain             TEXT NOT NULL,
    createdutc         TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE INDEX IF NOT EXISTS idx_cybo_workload_node_time
    ON cyboquatic_workload_telemetry (node_id, timestamputc);

CREATE INDEX IF NOT EXISTS idx_cybo_workload_plane_lane
    ON cyboquatic_workload_telemetry (canal_plane, fog_lane);

CREATE INDEX IF NOT EXISTS idx_cybo_workload_hex_subtask
    ON cyboquatic_workload_telemetry (phoenix_hex, domain, subtask_id);

DROP TRIGGER IF EXISTS trg_cybo_workload_ker_invariants;

CREATE TRIGGER trg_cybo_workload_ker_invariants
BEFORE INSERT ON cyboquatic_workload_telemetry
BEGIN
    SELECT CASE
        WHEN NEW.ker_k * NEW.ker_e - NEW.ker_r <= 0.0 THEN
            RAISE(ABORT, 'KER triad must yield positive corridor score')
    END;

    SELECT CASE
        WHEN NEW.delta_v_t > 0.05 THEN
            RAISE(ABORT, 'delta_v_t exceeds allowed Lyapunov corridor bound')
    END;

    SELECT CASE
        WHEN NEW.eco_efficiency < 0.7 AND NEW.fog_lane = 'PROD' THEN
            RAISE(ABORT, 'Production lane requires eco_efficiency >= 0.7')
    END;
END;


CREATE TABLE IF NOT EXISTS workload_daily_progress (
    progress_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    yyyymmdd          TEXT    NOT NULL,
    domain            TEXT    NOT NULL,
    subtask_id        TEXT    NOT NULL,
    node_id           TEXT    NOT NULL,
    sample_id         TEXT    NOT NULL,
    timestamp_utc     TEXT    NOT NULL,
    energy_req_j      REAL    NOT NULL,
    energy_surplus_j  REAL    NOT NULL,
    hydraulic_risk    REAL    NOT NULL,
    uncertainty_risk  REAL    NOT NULL,
    renergy           REAL    NOT NULL,
    rhydraulic        REAL    NOT NULL,
    runcertainty      REAL    NOT NULL,
    vt_before         REAL    NOT NULL,
    vt_after          REAL    NOT NULL,
    delta_vt          REAL    NOT NULL,
    k_factor          REAL    NOT NULL,
    e_factor          REAL    NOT NULL,
    r_factor          REAL    NOT NULL,
    phoenix_hex       TEXT    NOT NULL,
    prior_pointer     TEXT    NOT NULL,
    created_at        TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE INDEX IF NOT EXISTS idx_workload_daily_date
    ON workload_daily_progress (yyyymmdd);

CREATE INDEX IF NOT EXISTS idx_workload_daily_node_time
    ON workload_daily_progress (node_id, timestamp_utc);

CREATE INDEX IF NOT EXISTS idx_workload_daily_domain_subtask
    ON workload_daily_progress (domain, subtask_id);

CREATE INDEX IF NOT EXISTS idx_workload_daily_hex
    ON workload_daily_progress (phoenix_hex, node_id);


CREATE VIEW IF NOT EXISTS workload_daily_summary AS
SELECT
    yyyymmdd,
    domain,
    subtask_id,
    COUNT(*)                      AS sample_count,
    AVG(energy_req_j)             AS avg_energy_req_j,
    AVG(energy_surplus_j)         AS avg_energy_surplus_j,
    AVG(hydraulic_risk)           AS avg_hydraulic_risk,
    AVG(uncertainty_risk)         AS avg_uncertainty_risk,
    AVG(renergy)                  AS avg_renergy,
    AVG(rhydraulic)               AS avg_rhydraulic,
    AVG(runcertainty)             AS avg_runcertainty,
    AVG(vt_before)                AS avg_vt_before,
    AVG(vt_after)                 AS avg_vt_after,
    AVG(delta_vt)                 AS avg_delta_vt,
    AVG(k_factor)                 AS avg_k_factor,
    AVG(e_factor)                 AS avg_e_factor,
    AVG(r_factor)                 AS avg_r_factor
FROM workload_daily_progress
GROUP BY yyyymmdd, domain, subtask_id;


CREATE VIEW IF NOT EXISTS workload_high_risk_samples AS
SELECT
    progress_id,
    yyyymmdd,
    node_id,
    sample_id,
    timestamp_utc,
    r_factor,
    renergy,
    rhydraulic,
    runcertainty,
    vt_after,
    delta_vt
FROM workload_daily_progress
WHERE r_factor >= 0.7
ORDER BY timestamp_utc ASC;
