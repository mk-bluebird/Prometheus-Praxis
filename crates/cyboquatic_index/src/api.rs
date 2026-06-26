// filename: cyboquatic_index/src/api.rs
// destination: eco_restoration_shard/cyboquatic_index/src/api.rs

use rusqlite::{params, Connection, Result as SqlResult};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EnergyCarbonSummary {
    pub asset_code: String,
    pub site_code: String,
    pub window_start_utc: String,
    pub window_end_utc: String,
    pub energy_kwh: f64,
    pub co2e_kg: f64,
    pub solar_fraction: f64,
    pub corridor_status: String,
    pub decision_mode: String,
    pub vt_before_mean: f64,
    pub vt_after_mean: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EnergyCostOptimizationHint {
    pub asset_code: String,
    pub site_code: String,
    pub window_start_utc: String,
    pub window_end_utc: String,
    pub energy_kwh: f64,
    pub energy_cost_usd: f64,
    pub co2e_kg: f64,
    pub suggested_shift_hours: f64,
}

pub fn list_energy_carbon_windows(
    conn: &Connection,
    asset_code: &str,
    t_start: &str,
    t_end: &str,
) -> SqlResult<Vec<EnergyCarbonSummary>> {
    let mut stmt = conn.prepare(
        "SELECT a.asset_code, s.site_code,
                w.window_start_utc, w.window_end_utc,
                w.energy_kwh, w.co2e_kg,
                CASE WHEN w.energy_kwh > 0 THEN w.energy_kwh_solar / w.energy_kwh ELSE 0 END,
                w.corridor_status, w.decision_mode,
                w.vt_before_mean, w.vt_after_mean
         FROM cybo_workload_window w
         JOIN cybo_asset a ON a.asset_id = w.asset_id
         JOIN cybo_site s ON s.site_id = a.site_id
         WHERE a.asset_code = ?1
           AND w.window_start_utc >= ?2
           AND w.window_end_utc <= ?3
         ORDER BY w.window_start_utc",
    )?;

    let rows = stmt.query_map(params![asset_code, t_start, t_end], |row| {
        Ok(EnergyCarbonSummary {
            asset_code: row.get(0)?,
            site_code: row.get(1)?,
            window_start_utc: row.get(2)?,
            window_end_utc: row.get(3)?,
            energy_kwh: row.get(4)?,
            co2e_kg: row.get(5)?,
            solar_fraction: row.get(6)?,
            corridor_status: row.get(7)?,
            decision_mode: row.get(8)?,
            vt_before_mean: row.get(9)?,
            vt_after_mean: row.get(10)?,
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

pub fn estimate_energy_cost_shift(
    conn: &Connection,
    asset_code: &str,
    region_code: &str,
    window_start_utc: &str,
    window_end_utc: &str,
) -> SqlResult<Option<EnergyCostOptimizationHint>> {
    let summary = {
        let mut v = list_energy_carbon_windows(conn, asset_code, window_start_utc, window_end_utc)?;
        if v.is_empty() {
            return Ok(None);
        }
        v.remove(0)
    };

    let mut stmt = conn.prepare(
        "SELECT energy_price_usd_per_kwh, co2e_kg_per_kwh
         FROM cybo_energy_tariff
         WHERE region_code = ?1
           AND valid_from_utc <= ?2
           AND valid_to_utc >= ?3
         ORDER BY valid_from_utc DESC
         LIMIT 1",
    )?;

    let tariff = stmt
        .query_row(
            params![region_code, summary.window_start_utc, summary.window_end_utc],
            |row| Ok((row.get::<_, f64>(0)?, row.get::<_, f64>(1)?)),
        )
        .ok();

    if let Some((price, co2e_per_kwh)) = tariff {
        let energy_cost = summary.energy_kwh * price;
        let co2e = summary.energy_kwh * co2e_per_kwh;

        let hinted_shift = if price > 0.15 { 2.0 } else { 0.0 };

        Ok(Some(EnergyCostOptimizationHint {
            asset_code: summary.asset_code,
            site_code: summary.site_code,
            window_start_utc: summary.window_start_utc,
            window_end_utc: summary.window_end_utc,
            energy_kwh: summary.energy_kwh,
            energy_cost_usd: energy_cost,
            co2e_kg: co2e,
            suggested_shift_hours: hinted_shift,
        }))
    } else {
        Ok(None)
    }
}
