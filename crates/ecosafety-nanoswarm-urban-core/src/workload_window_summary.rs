// Filename: crates/ecosafety-nanoswarm-urban-core/src/workload_window_summary.rs
// rust-version = "1.85", edition = "2024"
// License: MIT OR Apache-2.0

//! Pure, non-actuating summary module for workload node windows.
//! Aligned with ecosafety.workload.window.summary.output.v1 and the SQL schema
//! in db/db_ecosafety_workload_window.sql.

/// Summary struct matching ecosafety.workload.window.summary.output.v1.
#[derive(Clone, Debug)]
pub struct WorkloadWindowSummary {
    pub nodeid: String,
    pub window_count: u32,
    pub mean_energy_req_j: f64,
    pub mean_energy_surplus_j: f64,
    pub mean_accepted_fraction: f64,
    pub mean_rejected_fraction: f64,
    pub mean_rerouted_fraction: f64,
    pub mean_vt_before: f64,
    pub mean_vt_after: f64,
    pub mean_delta_vt: f64,
    pub hard_violation_count: u32,
    pub soft_violation_count: u32,
}

/// Summarize workload node windows for a given node and time window.
/// This is a pure, read-only, non-actuating function.
///
/// # Arguments
/// * `conn` - A rusqlite database connection.
/// * `nodeid` - The node identifier to summarize.
/// * `window_start_utc` - Start of the time window (inclusive).
/// * `window_end_utc` - End of the time window (inclusive).
///
/// # Returns
/// Ok(None) if no rows match, Ok(Some(summary)) otherwise.
pub fn summarize_workload_node_window(
    conn: &rusqlite::Connection,
    nodeid: &str,
    window_start_utc: i64,
    window_end_utc: i64,
) -> rusqlite::Result<Option<WorkloadWindowSummary>> {
    // SQL query to compute aggregates for the given node and time window.
    let sql = r#"
        SELECT
            COUNT(*) as window_count,
            AVG(energy_req_j) as mean_energy_req_j,
            AVG(energy_surplus_j) as mean_energy_surplus_j,
            AVG(accepted_fraction) as mean_accepted_fraction,
            AVG(rejected_fraction) as mean_rejected_fraction,
            AVG(rerouted_fraction) as mean_rerouted_fraction,
            AVG(mean_vt_before) as mean_vt_before,
            AVG(mean_vt_after) as mean_vt_after,
            AVG(mean_delta_vt) as mean_delta_vt,
            SUM(CASE WHEN corridor_status = 'HARDVIOLATION' THEN 1 ELSE 0 END) as hard_violation_count,
            SUM(CASE WHEN corridor_status = 'SOFTVIOLATION' THEN 1 ELSE 0 END) as soft_violation_count
        FROM ecosafety_workload_node_window
        WHERE node_id = ?
          AND window_start_utc >= ?
          AND window_end_utc <= ?
    "#;

    let mut stmt = conn.prepare(sql)?;
    let result: Option<(
        i64,      // window_count
        Option<f64>, // mean_energy_req_j
        Option<f64>, // mean_energy_surplus_j
        Option<f64>, // mean_accepted_fraction
        Option<f64>, // mean_rejected_fraction
        Option<f64>, // mean_rerouted_fraction
        Option<f64>, // mean_vt_before
        Option<f64>, // mean_vt_after
        Option<f64>, // mean_delta_vt
        i64,      // hard_violation_count
        i64,      // soft_violation_count
    )> = stmt.query_row(
        rusqlite::params![nodeid, window_start_utc, window_end_utc],
        |row| {
            Ok((
                row.get(0)?,
                row.get(1)?,
                row.get(2)?,
                row.get(3)?,
                row.get(4)?,
                row.get(5)?,
                row.get(6)?,
                row.get(7)?,
                row.get(8)?,
                row.get(9)?,
                row.get(10)?,
            ))
        },
    ).optional()?;

    match result {
        None => Ok(None),
        Some((count, avg_e_req, avg_e_surp, avg_acc, avg_rej, avg_reroute, 
              avg_vt_before, avg_vt_after, avg_delta_vt, hard_count, soft_count)) => {
            if count == 0 {
                Ok(None)
            } else {
                // Handle potential NULL averages (shouldn't happen with count > 0, but be safe).
                let mean_energy_req_j = avg_e_req.unwrap_or(0.0);
                let mean_energy_surplus_j = avg_e_surp.unwrap_or(0.0);
                let mean_accepted_fraction = avg_acc.unwrap_or(0.0);
                let mean_rejected_fraction = avg_rej.unwrap_or(0.0);
                let mean_rerouted_fraction = avg_reroute.unwrap_or(0.0);
                let mean_vt_before = avg_vt_before.unwrap_or(0.0);
                let mean_vt_after = avg_vt_after.unwrap_or(0.0);
                let mean_delta_vt = avg_delta_vt.unwrap_or(0.0);

                Ok(Some(WorkloadWindowSummary {
                    nodeid: nodeid.to_string(),
                    window_count: count as u32,
                    mean_energy_req_j,
                    mean_energy_surplus_j,
                    mean_accepted_fraction,
                    mean_rejected_fraction,
                    mean_rerouted_fraction,
                    mean_vt_before,
                    mean_vt_after,
                    mean_delta_vt,
                    hard_violation_count: hard_count as u32,
                    soft_violation_count: soft_count as u32,
                }))
            }
        }
    }
}
