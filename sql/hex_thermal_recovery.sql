-- File: sql/hex_thermal_recovery.sql
-- Telemetry schema for heat-island recovery hex-cells, linked to cyboquatic telemetry.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS cyboquatic_workload_telemetry (
    telemetry_id INTEGER PRIMARY KEY AUTOINCREMENT,
    basin_id TEXT NOT NULL,
    timestamp_s REAL NOT NULL,
    flow_rate_m3_s REAL NOT NULL,
    head_m REAL NOT NULL,
    motor_efficiency REAL NOT NULL,
    aeration_factor REAL NOT NULL,
    energyreq_j REAL NOT NULL,
    delta_vt_m_s REAL NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_cybo_basin_time
    ON cyboquatic_workload_telemetry(basin_id, timestamp_s);

CREATE TABLE IF NOT EXISTS hex_cell_catalog (
    h3_index   TEXT PRIMARY KEY,
    basin_id   TEXT NOT NULL,
    center_lat REAL NOT NULL,
    center_lon REAL NOT NULL,
    FOREIGN KEY(basin_id) REFERENCES cyboquatic_workload_telemetry(basin_id)
);

CREATE INDEX IF NOT EXISTS idx_hex_basin
    ON hex_cell_catalog(basin_id);

CREATE TABLE IF NOT EXISTS hex_thermal_recovery (
    hex_recovery_id INTEGER PRIMARY KEY AUTOINCREMENT,
    h3_index        TEXT NOT NULL,
    basin_id        TEXT NOT NULL,
    date_utc        TEXT NOT NULL,      -- YYYY-MM-DD
    morning_lst_k   REAL NOT NULL,      -- morning LST [K]
    afternoon_lst_k REAL NOT NULL,      -- afternoon LST [K]
    albedo          REAL NOT NULL,      -- [0,1]
    cooling_degree_hours REAL NOT NULL,
    quality_flag    TEXT NOT NULL,      -- 'CLEAR', 'CLOUD', 'LOW_QUAL'
    lst_drop_c      REAL NOT NULL,      -- derived LST drop [°C]
    baseline_lst_c  REAL NOT NULL,
    period_start_utc INTEGER NOT NULL,
    period_end_utc   INTEGER NOT NULL,
    FOREIGN KEY(h3_index) REFERENCES hex_cell_catalog(h3_index),
    FOREIGN KEY(basin_id) REFERENCES cyboquatic_workload_telemetry(basin_id),
    CHECK (morning_lst_k   >= 250.0 AND morning_lst_k   <= 340.0),
    CHECK (afternoon_lst_k >= 250.0 AND afternoon_lst_k <= 360.0),
    CHECK (albedo >= 0.0 AND albedo <= 1.0),
    CHECK (cooling_degree_hours >= 0.0 AND cooling_degree_hours <= 650.0),
    CHECK (quality_flag IN ('CLEAR', 'CLOUD', 'LOW_QUAL')),
    CHECK (quality_flag <> 'CLOUD')
);

CREATE INDEX IF NOT EXISTS idx_hex_thermal_h3_date
    ON hex_thermal_recovery(h3_index, date_utc);

CREATE INDEX IF NOT EXISTS idx_hex_thermal_basin_date
    ON hex_thermal_recovery(basin_id, date_utc);

CREATE TABLE IF NOT EXISTS thermal_update_events (
    event_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    h3_index    TEXT NOT NULL,
    updated_utc INTEGER NOT NULL
);

CREATE TRIGGER IF NOT EXISTS hex_thermal_recovery_update_event
AFTER INSERT ON hex_thermal_recovery
FOR EACH ROW
BEGIN
    INSERT INTO thermal_update_events (h3_index, updated_utc)
    VALUES (NEW.h3_index, strftime('%s','now'));
END;

CREATE TABLE IF NOT EXISTS cross_correlation_matrix (
    segment_i       TEXT NOT NULL,
    segment_j       TEXT NOT NULL,
    lag_seconds     INTEGER NOT NULL,
    corr_value      REAL NOT NULL,
    window_start_utc INTEGER NOT NULL,
    window_end_utc   INTEGER NOT NULL,
    PRIMARY KEY (segment_i, segment_j, lag_seconds, window_start_utc, window_end_utc)
);

CREATE TABLE IF NOT EXISTS canal_network_lyapunov (
    window_start_utc INTEGER NOT NULL,
    window_end_utc   INTEGER NOT NULL,
    mode_index       INTEGER NOT NULL,
    exponent_value   REAL NOT NULL,
    PRIMARY KEY (window_start_utc, window_end_utc, mode_index)
);
