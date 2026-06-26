-- filename: db/db_cyboquatic_machinery_index.sql
-- destination: eco_restoration_shard/db/db_cyboquatic_machinery_index.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Cyboquatic site and asset taxonomy
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_site (
  site_id        INTEGER PRIMARY KEY AUTOINCREMENT,
  region_code    TEXT NOT NULL,      -- e.g. "Phoenix-AZ"
  basin_code     TEXT NOT NULL,      -- e.g. "SRP-CANAL-NW"
  site_code      TEXT NOT NULL,      -- e.g. "PHX-CANAL-SEG-07"
  site_kind      TEXT NOT NULL,      -- CANAL_SEGMENT, MAR_VAULT, LIFT_STATION, AWP_OUTFALL
  latitude_deg   REAL NOT NULL,
  longitude_deg  REAL NOT NULL,
  elevation_m    REAL NOT NULL,
  is_sensitive   INTEGER NOT NULL DEFAULT 0,  -- 1 if BioticTreaty / Indigenous protected reach
  created_utc    TEXT NOT NULL,
  updated_utc    TEXT NOT NULL,
  UNIQUE (region_code, site_code)
);

CREATE TABLE IF NOT EXISTS cybo_asset (
  asset_id        INTEGER PRIMARY KEY AUTOINCREMENT,
  site_id         INTEGER NOT NULL REFERENCES cybo_site(site_id) ON DELETE CASCADE,
  asset_code      TEXT NOT NULL,     -- e.g. "PUMP-07-A", "GATE-07-WEIR1"
  asset_kind      TEXT NOT NULL,     -- PUMP, VALVE, GATE, SENSOR_RACK, SOFTROBOT_NODE
  manufacturer    TEXT,
  model_code      TEXT,
  rated_flow_m3h  REAL,
  rated_head_m    REAL,
  rated_power_kw  REAL,
  install_year    INTEGER,
  criticality     TEXT NOT NULL DEFAULT 'NORMAL',  -- NORMAL, HIGH, EXTREME
  ecosafety_corr_id TEXT,           -- link into ecosafety corridor set (ALN id)
  ecosafety_guard_label TEXT,       -- link into guard policy label
  created_utc     TEXT NOT NULL,
  updated_utc     TEXT NOT NULL,
  UNIQUE (site_id, asset_code)
);

CREATE INDEX IF NOT EXISTS idx_cybo_asset_site_kind
  ON cybo_asset(site_id, asset_kind);

----------------------------------------------------------------------
-- 2. Instrumentation profile (non-actuating)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_instrument_profile (
  profile_id     INTEGER PRIMARY KEY AUTOINCREMENT,
  asset_id       INTEGER NOT NULL REFERENCES cybo_asset(asset_id) ON DELETE CASCADE,
  sensor_code    TEXT NOT NULL,   -- FLOW, LEVEL, VIBRATION, TEMP, POWER, PH, TURBIDITY
  unit           TEXT NOT NULL,   -- m3/h, %, g, C, kW, mg/L, NTU
  sample_period_s INTEGER NOT NULL,
  last_cal_utc   TEXT,
  created_utc    TEXT NOT NULL,
  UNIQUE (asset_id, sensor_code)
);

----------------------------------------------------------------------
-- 3. Eco corridor + Lyapunov binding (summary view)
--    These rows mirror the ALN ecosafety grammar for quick lookup.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_ecosafety_binding (
  binding_id        INTEGER PRIMARY KEY AUTOINCREMENT,
  asset_id          INTEGER NOT NULL REFERENCES cybo_asset(asset_id) ON DELETE CASCADE,
  corridor_id       TEXT NOT NULL,  -- ALN CorridorId (e.g. "CORRIDORBIO-FLOWVACSUBSTRATEV1")
  domain_name       TEXT NOT NULL,  -- SOFTROBOT, MAR, WATER, MATERIAL
  geography_id      TEXT NOT NULL,  -- REGIONDOWNTOWNCANALEDGE, etc.
  vt_monotone       INTEGER NOT NULL DEFAULT 1,
  vt_min            REAL NOT NULL,
  vt_max            REAL NOT NULL,
  vt_tolerance      REAL NOT NULL,
  rx_rdegrade_min   REAL,
  rx_rdegrade_max   REAL,
  rx_rresidual_min  REAL,
  rx_rresidual_max  REAL,
  rx_rmicro_min     REAL,
  rx_rmicro_max     REAL,
  rx_rtoxacute_min  REAL,
  rx_rtoxacute_max  REAL,
  rx_rtoxchron_min  REAL,
  rx_rtoxchron_max  REAL,
  created_utc       TEXT NOT NULL,
  updated_utc       TEXT NOT NULL,
  UNIQUE (asset_id, corridor_id)
);

CREATE INDEX IF NOT EXISTS idx_cybo_ecosafety_asset
  ON cybo_ecosafety_binding(asset_id);

----------------------------------------------------------------------
-- 4. Energy / carbon / hydraulic workload windows
--    Non-actuating log derived from plant/SCADA exports.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_workload_window (
  window_id        INTEGER PRIMARY KEY AUTOINCREMENT,
  asset_id         INTEGER NOT NULL REFERENCES cybo_asset(asset_id) ON DELETE CASCADE,
  window_start_utc TEXT NOT NULL,
  window_end_utc   TEXT NOT NULL,
  total_runtime_s  INTEGER NOT NULL,
  on_cycles        INTEGER NOT NULL,
  start_stop_events INTEGER NOT NULL,
  flow_through_m3  REAL,
  elev_gain_m      REAL,
  energy_kwh       REAL,
  energy_kwh_solar REAL,
  energy_kwh_grid  REAL,
  co2e_kg          REAL,
  vt_before_mean   REAL,
  vt_after_mean    REAL,
  rx_rdegrade_mean REAL,
  rx_rmicro_mean   REAL,
  rcarbon_mean     REAL,
  rbiodiv_mean     REAL,
  corridor_status  TEXT NOT NULL,  -- SATISFIED, SOFTVIOLATION, HARDVIOLATION
  decision_mode    TEXT NOT NULL,  -- ALLOW, DERATE, STOP
  created_utc      TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_cybo_workload_asset_time
  ON cybo_workload_window(asset_id, window_start_utc, window_end_utc);

----------------------------------------------------------------------
-- 5. Energy cost + tariff snapshots (for "cost of energy" analytics)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_energy_tariff (
  tariff_id       INTEGER PRIMARY KEY AUTOINCREMENT,
  region_code     TEXT NOT NULL,
  provider_name   TEXT NOT NULL,
  tariff_code     TEXT NOT NULL,
  valid_from_utc  TEXT NOT NULL,
  valid_to_utc    TEXT NOT NULL,
  energy_price_usd_per_kwh REAL NOT NULL,
  co2e_kg_per_kwh REAL NOT NULL,
  renewable_fraction REAL NOT NULL,
  created_utc     TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_cybo_energy_tariff_region_time
  ON cybo_energy_tariff(region_code, valid_from_utc, valid_to_utc);

----------------------------------------------------------------------
-- 6. Blast-radius cross-link into eco_restoration_index (if present)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_blastradius_bridge (
  bridge_id       INTEGER PRIMARY KEY AUTOINCREMENT,
  asset_id        INTEGER NOT NULL REFERENCES cybo_asset(asset_id) ON DELETE CASCADE,
  shard_id        INTEGER NOT NULL,  -- foreign key into eco_restoration_index.shardinstance
  link_impacttype TEXT NOT NULL,     -- HYDRAULIC, ENERGY, CARBON, BIODIVERSITY
  link_impactscore REAL NOT NULL,
  link_vtsensitivity REAL,
  created_utc     TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_cybo_blast_asset
  ON cybo_blastradius_bridge(asset_id, link_impacttype);
