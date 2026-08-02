-- File: sql/system_workload_core_schema.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS system_node (
    node_id        INTEGER PRIMARY KEY,
    hex_id         TEXT NOT NULL,
    process_stage  TEXT NOT NULL,
    description    TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS system_asset (
    asset_id                          INTEGER PRIMARY KEY,
    asset_type                        TEXT NOT NULL,
    rated_flow_m3h                    REAL NOT NULL CHECK (rated_flow_m3h >= 0.0),
    energy_efficiency_kWh_per_m3      REAL NOT NULL CHECK (energy_efficiency_kWh_per_m3 >= 0.0),
    carbon_factor_kgCO2_per_kWh       REAL NOT NULL CHECK (carbon_factor_kgCO2_per_kWh >= 0.0),
    vfd_status                        TEXT NOT NULL CHECK (vfd_status IN ('NONE', 'INSTALLED', 'PLANNED')),
    scada_tag_group                   TEXT NOT NULL,
    created_utc                       TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc                       TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TABLE IF NOT EXISTS system_workload (
    workload_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    node_id        INTEGER NOT NULL REFERENCES system_node(node_id) ON DELETE CASCADE,
    window_label   TEXT NOT NULL,
    timestamp_utc  TEXT NOT NULL,
    flow_m3h       REAL NOT NULL CHECK (flow_m3h >= 0.0),
    load_cod_mgL   REAL NOT NULL CHECK (load_cod_mgL >= 0.0),
    pfas_ngL       REAL NOT NULL CHECK (pfas_ngL >= 0.0),
    created_utc    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TABLE IF NOT EXISTS system_workload_plan (
    plan_id             INTEGER PRIMARY KEY AUTOINCREMENT,
    window_label        TEXT NOT NULL,
    node_id             INTEGER NOT NULL REFERENCES system_node(node_id) ON DELETE CASCADE,
    asset_id            INTEGER NOT NULL REFERENCES system_asset(asset_id) ON DELETE CASCADE,
    assigned_flow_m3h   REAL NOT NULL CHECK (assigned_flow_m3h >= 0.0),
    energy_kWh          REAL NOT NULL CHECK (energy_kWh >= 0.0),
    carbon_kgCO2        REAL NOT NULL CHECK (carbon_kgCO2 >= 0.0),
    created_utc         TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TABLE IF NOT EXISTS system_energy (
    energy_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    asset_id        INTEGER NOT NULL REFERENCES system_asset(asset_id) ON DELETE CASCADE,
    timestamp_utc   TEXT NOT NULL,
    energy_kWh      REAL NOT NULL CHECK (energy_kWh >= 0.0),
    volume_m3       REAL NOT NULL CHECK (volume_m3 >= 0.0),
    window_label    TEXT NOT NULL,
    created_utc     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TABLE IF NOT EXISTS system_carbon (
    carbon_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    asset_id        INTEGER NOT NULL REFERENCES system_asset(asset_id) ON DELETE CASCADE,
    timestamp_utc   TEXT NOT NULL,
    carbon_kgCO2    REAL NOT NULL CHECK (carbon_kgCO2 >= 0.0),
    window_label    TEXT NOT NULL,
    created_utc     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TABLE IF NOT EXISTS system_risk (
    risk_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    node_id         INTEGER NOT NULL REFERENCES system_node(node_id) ON DELETE CASCADE,
    timestamp_utc   TEXT NOT NULL,
    r_hyd           REAL NOT NULL CHECK (r_hyd >= 0.0 AND r_hyd <= 1.0),
    r_ene           REAL NOT NULL CHECK (r_ene >= 0.0 AND r_ene <= 1.0),
    r_top           REAL NOT NULL CHECK (r_top >= 0.0 AND r_top <= 1.0),
    r_bio           REAL NOT NULL CHECK (r_bio >= 0.0 AND r_bio <= 1.0),
    r_pfas          REAL NOT NULL CHECK (r_pfas >= 0.0 AND r_pfas <= 1.0),
    window_label    TEXT NOT NULL,
    created_utc     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE VIEW IF NOT EXISTS v_system_kwh_per_m3 AS
SELECT
    e.window_label,
    a.asset_type,
    a.asset_id,
    SUM(e.energy_kWh) AS total_energy_kWh,
    SUM(e.volume_m3) AS total_volume_m3,
    CASE
        WHEN SUM(e.volume_m3) > 0.0 THEN SUM(e.energy_kWh) / SUM(e.volume_m3)
        ELSE 0.0
    END AS kWh_per_m3
FROM system_energy e
JOIN system_asset a ON a.asset_id = e.asset_id
GROUP BY e.window_label, a.asset_type, a.asset_id;

CREATE VIEW IF NOT EXISTS v_system_carbon_balance AS
SELECT
    c.window_label,
    a.asset_type,
    a.asset_id,
    SUM(c.carbon_kgCO2) AS total_carbon_kgCO2
FROM system_carbon c
JOIN system_asset a ON a.asset_id = c.asset_id
GROUP BY c.window_label, a.asset_type, a.asset_id;

CREATE VIEW IF NOT EXISTS v_system_risk_corridor AS
SELECT
    window_label,
    node_id,
    MAX(r_hyd) AS r_hyd_max,
    MAX(r_pfas) AS r_pfas_max,
    MAX(r_bio) AS r_bio_max
FROM system_risk
GROUP BY window_label, node_id;
