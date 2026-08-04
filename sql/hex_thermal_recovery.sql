-- File: sql/hex_thermal_recovery.sql
-- Telemetry schema for heat-island recovery hex-cells, linked to cyboquatic telemetry.

PRAGMA foreign_keys = ON;

-- Core cyboquatic telemetry table (assumed existing, referenced here for completeness).
-- This stores basin-level workload and thermal metrics per sample.
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

-- Hex-cell catalog table mapping H3 indices to basins.
-- This enables a spatial join between hex cells and cyboquatic telemetry.
CREATE TABLE IF NOT EXISTS hex_cell_catalog (
    h3_index TEXT PRIMARY KEY,     -- H3 index string for hex cell
    basin_id TEXT NOT NULL,        -- basin associated with this hex
    center_lat REAL NOT NULL,
    center_lon REAL NOT NULL,
    FOREIGN KEY(basin_id) REFERENCES cyboquatic_workload_telemetry(basin_id)
);

CREATE INDEX IF NOT EXISTS idx_hex_basin
    ON hex_cell_catalog(basin_id);

-- Heat-island recovery telemetry table for hex cells.
-- Fields:
--  - morning_lst_k: land-surface temperature (Kelvin) in morning (e.g., pre-sunrise)
--  - afternoon_lst_k: land-surface temperature (Kelvin) in afternoon (e.g., mid-afternoon)
--  - albedo: dimensionless reflectance [0,1]
--  - cooling_degree_hours: integrated "cooling" relative to a reference temperature over a day
-- Physics-based maximum:
--  For a given cell, cooling-degree-hours (CDH) are bounded by:
--     CDH_max = (T_ref - T_min_cell) * 24
--  where T_min_cell is the minimum plausible canopy temperature (e.g., 273.15 K for 0°C),
--  and T_ref is a reference comfort temperature (e.g., 300 K ~ 27°C).
--  We enforce a conservative upper bound via a CHECK constraint using fixed T_ref and T_min_cell.

CREATE TABLE IF NOT EXISTS hex_thermal_recovery (
    hex_recovery_id INTEGER PRIMARY KEY AUTOINCREMENT,
    h3_index TEXT NOT NULL,
    basin_id TEXT NOT NULL,
    date_utc TEXT NOT NULL,             -- ISO-8601 date string (YYYY-MM-DD)
    morning_lst_k REAL NOT NULL,        -- morning land-surface temperature [K]
    afternoon_lst_k REAL NOT NULL,      -- afternoon land-surface temperature [K]
    albedo REAL NOT NULL,               -- dimensionless [0,1]
    cooling_degree_hours REAL NOT NULL, -- integrated cooling degree-hours
    -- Foreign key to hex cell catalog for spatial join
    FOREIGN KEY(h3_index) REFERENCES hex_cell_catalog(h3_index),
    FOREIGN KEY(basin_id) REFERENCES cyboquatic_workload_telemetry(basin_id),
    -- Physical plausibility constraints:
    CHECK (morning_lst_k >= 250.0 AND morning_lst_k <= 340.0),
    CHECK (afternoon_lst_k >= 250.0 AND afternoon_lst_k <= 360.0),
    CHECK (albedo >= 0.0 AND albedo <= 1.0),
    -- Cooling-degree-hours maximum constraint:
    -- Assume reference temperature T_ref = 300 K, minimum physical cell temperature T_min_cell = 273.15 K.
    -- Then CDH_max = (T_ref - T_min_cell) * 24 ≈ (26.85 K) * 24 ≈ 644.4 K·h.
    -- We enforce a slightly conservative bound of 650 K·h.
    CHECK (cooling_degree_hours >= 0.0 AND cooling_degree_hours <= 650.0)
);

-- Spatial-temporal index to accelerate queries joining H3 hexes and dates.
CREATE INDEX IF NOT EXISTS idx_hex_thermal_h3_date
    ON hex_thermal_recovery(h3_index, date_utc);

CREATE INDEX IF NOT EXISTS idx_hex_thermal_basin_date
    ON hex_thermal_recovery(basin_id, date_utc);
