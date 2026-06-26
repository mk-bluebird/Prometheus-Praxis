// filename: cyboquatic_index/src/migration.rs
// destination: eco_restoration_shard/cyboquatic_index/src/migration.rs

use rusqlite::{params, Connection, Result as SqlResult};

pub fn run_all_migrations(conn: &Connection) -> SqlResult<()> {
    conn.execute_batch(include_str!("../sql/db_cyboquatic_machinery_index.sql"))?;
    seed_phoenix_examples(conn)?;
    Ok(())
}

fn seed_phoenix_examples(conn: &Connection) -> SqlResult<()> {
    let now = "2026-01-01T00:00:00Z";

    // Canal segment with Cyboquatic pumps
    conn.execute(
        "INSERT OR IGNORE INTO cybo_site
         (region_code, basin_code, site_code, site_kind,
          latitude_deg, longitude_deg, elevation_m, is_sensitive,
          created_utc, updated_utc)
         VALUES
         ('Phoenix-AZ','SRP-CANAL-NW','PHX-CANAL-SEG-07','CANAL_SEGMENT',
          33.4940,-112.0830, 340.0, 1, ?, ?)",
        params![now, now],
    )?;

    // MAR vault
    conn.execute(
        "INSERT OR IGNORE INTO cybo_site
         (region_code, basin_code, site_code, site_kind,
          latitude_deg, longitude_deg, elevation_m, is_sensitive,
          created_utc, updated_utc)
         VALUES
         ('Phoenix-AZ','SRP-CANAL-NW','PHX-MAR-VAULT-01','MAR_VAULT',
          33.4600,-112.1200, 335.0, 1, ?, ?)",
        params![now, now],
    )?;

    let canal_site_id: i64 = conn.query_row(
        "SELECT site_id FROM cybo_site WHERE site_code = 'PHX-CANAL-SEG-07'",
        [],
        |r| r.get(0),
    )?;
    let mar_site_id: i64 = conn.query_row(
        "SELECT site_id FROM cybo_site WHERE site_code = 'PHX-MAR-VAULT-01'",
        [],
        |r| r.get(0),
    )?;

    // Pump asset at canal segment
    conn.execute(
        "INSERT OR IGNORE INTO cybo_asset
         (site_id, asset_code, asset_kind, manufacturer, model_code,
          rated_flow_m3h, rated_head_m, rated_power_kw,
          install_year, criticality, ecosafety_corr_id, ecosafety_guard_label,
          created_utc, updated_utc)
         VALUES
         (?1, 'PUMP-07-A', 'PUMP', 'Aletheion-Industrial', 'PHX-CYBO-PUMP-10',
          900.0, 12.0, 22.0,
          2025, 'EXTREME', 'CORRIDORBIO-FLOWVACSUBSTRATEV1', 'POLICY_SOFTROBOT',
          ?, ?)",
        params![canal_site_id, now, now],
    )?;

    // Biodegradable softrobot node near MAR vault
    conn.execute(
        "INSERT OR IGNORE INTO cybo_asset
         (site_id, asset_code, asset_kind, manufacturer, model_code,
          rated_flow_m3h, rated_head_m, rated_power_kw,
          install_year, criticality, ecosafety_corr_id, ecosafety_guard_label,
          created_utc, updated_utc)
         VALUES
         (?1, 'SOFTBOT-MAR-01', 'SOFTROBOT_NODE', 'Aletheion-Lab', 'SOFTBOT-BIO-01',
          NULL, NULL, NULL,
          2026, 'HIGH', 'CORRIDORBIO-SOFTBOTSHELLV1', 'POLICY_SOFTROBOT',
          ?, ?)",
        params![mar_site_id, now, now],
    )?;

    let pump_asset_id: i64 = conn.query_row(
        "SELECT asset_id FROM cybo_asset
         WHERE site_id = ?1 AND asset_code = 'PUMP-07-A'",
        params![canal_site_id],
        |r| r.get(0),
    )?;
    let softbot_asset_id: i64 = conn.query_row(
        "SELECT asset_id FROM cybo_asset
         WHERE site_id = ?1 AND asset_code = 'SOFTBOT-MAR-01'",
        params![mar_site_id],
        |r| r.get(0),
    )?;

    // Ecosafety bindings (numbers aligned with biodegradable corridor bands)
    conn.execute(
        "INSERT OR IGNORE INTO cybo_ecosafety_binding
         (asset_id, corridor_id, domain_name, geography_id,
          vt_monotone, vt_min, vt_max, vt_tolerance,
          rx_rdegrade_min, rx_rdegrade_max,
          rx_rresidual_min, rx_rresidual_max,
          rx_rmicro_min, rx_rmicro_max,
          rx_rtoxacute_min, rx_rtoxacute_max,
          rx_rtoxchron_min, rx_rtoxchron_max,
          created_utc, updated_utc)
         VALUES
         (?1,'CORRIDORBIO-FLOWVACSUBSTRATEV1','SOFTROBOT','REGIONDOWNTOWNCANALEDGE',
          1, 0.0, 1.0, 1e-3,
          0.40, 0.95,
          0.00, 0.20,
          0.00, 0.10,
          0.00, 0.10,
          0.00, 0.15,
          ?, ?)",
        params![pump_asset_id, now, now],
    )?;

    conn.execute(
        "INSERT OR IGNORE INTO cybo_ecosafety_binding
         (asset_id, corridor_id, domain_name, geography_id,
          vt_monotone, vt_min, vt_max, vt_tolerance,
          rx_rdegrade_min, rx_rdegrade_max,
          rx_rresidual_min, rx_rresidual_max,
          rx_rmicro_min, rx_rmicro_max,
          rx_rtoxacute_min, rx_rtoxacute_max,
          rx_rtoxchron_min, rx_rtoxchron_max,
          created_utc, updated_utc)
         VALUES
         (?1,'CORRIDORBIO-SOFTBOTSHELLV1','SOFTROBOT','REGIONDOWNTOWNCORE',
          1, 0.0, 1.0, 1e-3,
          0.40, 0.95,
          0.00, 0.20,
          0.00, 0.10,
          0.00, 0.10,
          0.00, 0.15,
          ?, ?)",
        params![softbot_asset_id, now, now],
    )?;

    // Example workload windows for the pump
    conn.execute(
        "INSERT INTO cybo_workload_window
         (asset_id, window_start_utc, window_end_utc,
          total_runtime_s, on_cycles, start_stop_events,
          flow_through_m3, elev_gain_m,
          energy_kwh, energy_kwh_solar, energy_kwh_grid,
          co2e_kg, vt_before_mean, vt_after_mean,
          rx_rdegrade_mean, rx_rmicro_mean,
          rcarbon_mean, rbiodiv_mean,
          corridor_status, decision_mode, created_utc)
         VALUES
         (?1,'2026-07-01T00:00:00Z','2026-07-01T23:59:59Z',
          28800, 24, 24,
          18000.0, 12.0,
          220.0, 150.0, 70.0,
          40.0, 0.38, 0.37,
          0.70, 0.05,
          0.20, 0.10,
          'SATISFIED','ALLOW', ?)",
        params![pump_asset_id, now],
    )?;

    Ok(())
}
