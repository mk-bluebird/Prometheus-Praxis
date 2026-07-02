// Filename: crates/ecosafety-nanoswarm-urban-core/src/workload_window_query.rs
// rust-version = "1.85", edition = "2024"
// License: MIT OR Apache-2.0

//! Pure, non-actuating query module for listing workload node windows.
//! Aligned with ecosafety.workload.window.list.input.v1 and the SQL schema
//! in db/db_ecosafety_workload_window.sql.

use crate::workload_window::WorkloadNodeWindow;
use crate::types::KerTriplet;

/// Filter struct matching ecosafety.workload.window.list.input.v1.
/// All fields are optional; only Some values are applied as filters.
#[derive(Clone, Debug, Default)]
pub struct WorkloadWindowFilter {
    pub shardid: Option<String>,
    pub nodeid: Option<String>,
    pub assetid: Option<String>,
    pub window_start_utc_min: Option<i64>,
    pub window_end_utc_max: Option<i64>,
}

/// List workload node windows from the database, applying optional filters.
/// This is a pure, read-only, non-actuating function.
///
/// # Arguments
/// * `conn` - A rusqlite database connection.
/// * `filter` - Optional filters; only Some fields are applied.
///
/// # Returns
/// A Vec of WorkloadNodeWindow rows matching the filter criteria.
pub fn list_workload_node_windows(
    conn: &rusqlite::Connection,
    filter: &WorkloadWindowFilter,
) -> rusqlite::Result<Vec<WorkloadNodeWindow>> {
    // Build the base SELECT query.
    let mut sql = String::from(
        "SELECT \
            shardid, timestamputc, objectid, \
            node_id, asset_id, \
            window_start_utc, window_end_utc, \
            energy_req_j, energy_surplus_j, \
            accepted_fraction, rejected_fraction, rerouted_fraction, \
            mean_vt_before, mean_vt_after, mean_delta_vt, \
            mean_r_carbon, mean_r_biodiv, \
            corridor_status, decision_mode, \
            ker_k, ker_e, ker_r \
         FROM ecosafety_workload_node_window \
         WHERE 1=1"
    );

    // Collect parameters for the prepared statement.
    let mut params: Vec<&dyn rusqlite::ToSql> = Vec::new();

    // Apply optional filters.
    if let Some(ref shardid) = filter.shardid {
        sql.push_str(" AND shardid = ?");
        params.push(shardid);
    }
    if let Some(ref nodeid) = filter.nodeid {
        sql.push_str(" AND node_id = ?");
        params.push(nodeid);
    }
    if let Some(ref assetid) = filter.assetid {
        sql.push_str(" AND asset_id = ?");
        params.push(assetid);
    }
    if let Some(window_start_min) = filter.window_start_utc_min {
        sql.push_str(" AND window_start_utc >= ?");
        params.push(&window_start_min);
    }
    if let Some(window_end_max) = filter.window_end_utc_max {
        sql.push_str(" AND window_end_utc <= ?");
        params.push(&window_end_max);
    }

    // Prepare and execute the query.
    let mut stmt = conn.prepare(&sql)?;
    let rows = stmt.query_map(rusqlite::params_from_iter(params.iter()), |row| {
        let shardid: String = row.get(0)?;
        let timestamputc: i64 = row.get(1)?;
        let objectid: String = row.get(2)?;

        let node_id: String = row.get(3)?;
        let asset_id: Option<String> = row.get(4)?;

        let window_start_utc: i64 = row.get(5)?;
        let window_end_utc: i64 = row.get(6)?;

        let energy_req_j: f64 = row.get(7)?;
        let energy_surplus_j: f64 = row.get(8)?;

        let accepted_fraction: f64 = row.get(9)?;
        let rejected_fraction: f64 = row.get(10)?;
        let rerouted_fraction: f64 = row.get(11)?;

        let mean_vt_before: f64 = row.get(12)?;
        let mean_vt_after: f64 = row.get(13)?;
        let mean_delta_vt: f64 = row.get(14)?;

        let mean_r_carbon: Option<f64> = row.get(15)?;
        let mean_r_biodiv: Option<f64> = row.get(16)?;

        let corridor_status: String = row.get(17)?;
        let decision_mode: String = row.get(18)?;

        let ker_k: f64 = row.get(19)?;
        let ker_e: f64 = row.get(20)?;
        let ker_r: f64 = row.get(21)?;

        let base_ker = KerTriplet { k: ker_k, e: ker_e, r: ker_r };

        // Construct WorkloadNodeWindow using the build method.
        Ok(WorkloadNodeWindow::build(
            shardid,
            timestamputc,
            objectid,
            base_ker,
            node_id,
            asset_id,
            window_start_utc,
            window_end_utc,
            energy_req_j,
            energy_surplus_j,
            accepted_fraction,
            rejected_fraction,
            rerouted_fraction,
            mean_vt_before,
            mean_vt_after,
            mean_r_carbon,
            mean_r_biodiv,
            corridor_status,
            decision_mode,
        ))
    })?;

    // Collect results into a Vec.
    let mut results = Vec::new();
    for row_result in rows {
        results.push(row_result?);
    }

    Ok(results)
}
