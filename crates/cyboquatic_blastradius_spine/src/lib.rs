// filename: crates/cyboquatic_blastradius_spine/src/lib.rs
// destination: eco_restoration_shard/crates/cyboquatic_blastradius_spine/src/lib.rs

#![forbid(unsafe_code)]

use chrono::{DateTime, Utc};
use rusqlite::{params, Connection};
use serde::{Deserialize, Serialize};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum SpineError {
    #[error("SQLite error: {0}")]
    Sqlite(#[from] rusqlite::Error),
    #[error("Time parse error: {0}")]
    TimeParse(#[from] chrono::ParseError),
}

/// Blast-radius aggregation for a shard.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ShardBlastRadius {
    pub shard_id: String,
    pub max_node_radius: f64,
    pub max_material_radius: f64,
    pub max_carbon_radius: f64,
    pub max_biodiv_radius: f64,
    pub vt_radius_sum: f64,
}

/// Blast-radius aggregation for a machine.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MachineBlastRadius {
    pub machine_id: String,
    pub max_node_radius: f64,
    pub max_region_radius: f64,
    pub max_energy_radius: f64,
    pub max_carbon_radius: f64,
    pub vt_radius_sum: f64,
}

/// Workload summary over a node+region window.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkloadNodeWindow {
    pub node_id: String,
    pub region: String,
    pub window_start_utc: DateTime<Utc>,
    pub window_end_utc: DateTime<Utc>,
    pub total_req_j: f64,
    pub total_surplus_j: f64,
    pub mean_vt_before: f64,
    pub mean_vt_after: f64,
    pub mean_r_carbon: Option<f64>,
    pub mean_r_biodiv: Option<f64>,
    pub accepts: u64,
    pub rejects: u64,
    pub reroutes: u64,
    /// Simple diagnostic: ΔV_t = mean_vt_after - mean_vt_before
    pub delta_vt: f64,
    /// Accept fraction = accepts / (accepts + rejects + reroutes)
    pub accept_fraction: f64,
}

/// Readonly spine handle.
pub struct CyboSpine {
    conn: Connection,
}

impl CyboSpine {
    /// Open an existing SQLite DB containing the blastradius + workload ledger schema.
    pub fn open(db_path: &str) -> Result<Self, SpineError> {
        let conn = Connection::open(db_path)?;
        conn.execute_batch("PRAGMA foreign_keys = ON;")?;
        Ok(CyboSpine { conn })
    }

    /// List shard-level blast radius aggregates.
    pub fn list_shard_blastradius(&self) -> Result<Vec<ShardBlastRadius>, SpineError> {
        let mut stmt = self.conn.prepare(
            r#"SELECT shard_id,
                       max_node_radius,
                       max_material_radius,
                       max_carbon_radius,
                       max_biodiv_radius,
                       vt_radius_sum
                FROM v_shard_blastradius"#,
        )?;
        let rows = stmt.query_map([], |row| {
            Ok(ShardBlastRadius {
                shard_id: row.get(0)?,
                max_node_radius: row.get(1)?,
                max_material_radius: row.get(2)?,
                max_carbon_radius: row.get(3)?,
                max_biodiv_radius: row.get(4)?,
                vt_radius_sum: row.get(5)?,
            })
        })?;

        let mut out = Vec::new();
        for r in rows {
            out.push(r?);
        }
        Ok(out)
    }

    /// List machine-level blast radius aggregates.
    pub fn list_machine_blastradius(&self) -> Result<Vec<MachineBlastRadius>, SpineError> {
        let mut stmt = self.conn.prepare(
            r#"SELECT machine_id,
                       max_node_radius,
                       max_region_radius,
                       max_energy_radius,
                       max_carbon_radius,
                       vt_radius_sum
                FROM v_machine_blastradius"#,
        )?;
        let rows = stmt.query_map([], |row| {
            Ok(MachineBlastRadius {
                machine_id: row.get(0)?,
                max_node_radius: row.get(1)?,
                max_region_radius: row.get(2)?,
                max_energy_radius: row.get(3)?,
                max_carbon_radius: row.get(4)?,
                vt_radius_sum: row.get(5)?,
            })
        })?;

        let mut out = Vec::new();
        for r in rows {
            out.push(r?);
        }
        Ok(out)
    }

    /// Summarize workloads for a given node+region over all recorded time.
    pub fn summarize_workload_node_region(
        &self,
        node_id: &str,
        region: &str,
    ) -> Result<Option<WorkloadNodeWindow>, SpineError> {
        let mut stmt = self.conn.prepare(
            r#"SELECT node_id,
                       region,
                       window_start_utc,
                       window_end_utc,
                       total_req_j,
                       total_surplus_j,
                       mean_vt_before,
                       mean_vt_after,
                       mean_r_carbon,
                       mean_r_biodiv,
                       accepts,
                       rejects,
                       reroutes
                FROM v_cybo_workload_node_window
                WHERE node_id = ?1 AND region = ?2"#,
        )?;

        let opt_row = stmt.query_row(params![node_id, region], |row| {
            let start_str: String = row.get(2)?;
            let end_str: String = row.get(3)?;
            let start = start_str.parse::<DateTime<Utc>>()?;
            let end = end_str.parse::<DateTime<Utc>>()?;

            let accepts: i64 = row.get(10)?;
            let rejects: i64 = row.get(11)?;
            let reroutes: i64 = row.get(12)?;
            let total = (accepts + rejects + reroutes) as f64;
            let accept_fraction = if total > 0.0 {
                accepts as f64 / total
            } else {
                0.0
            };

            let mean_vt_before: f64 = row.get(6)?;
            let mean_vt_after: f64 = row.get(7)?;
            let delta_vt = mean_vt_after - mean_vt_before;

            Ok(WorkloadNodeWindow {
                node_id: row.get(0)?,
                region: row.get(1)?,
                window_start_utc: start,
                window_end_utc: end,
                total_req_j: row.get(4)?,
                total_surplus_j: row.get(5)?,
                mean_vt_before,
                mean_vt_after,
                mean_r_carbon: row.get(8)?,
                mean_r_biodiv: row.get(9)?,
                accepts: accepts as u64,
                rejects: rejects as u64,
                reroutes: reroutes as u64,
                delta_vt,
                accept_fraction,
            })
        });

        match opt_row {
            Ok(summary) => Ok(Some(summary)),
            Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
            Err(e) => Err(SpineError::Sqlite(e)),
        }
    }
}

//
// FFI surface for Lua / C++ / Kotlin / Android (readonly, JSON-based)
//

#[no_mangle]
pub extern "C" fn cybo_spine_list_shard_blastradius_json(
    db_path_ptr: *const i8,
) -> *mut i8 {
    use std::ffi::{CStr, CString};

    if db_path_ptr.is_null() {
        return std::ptr::null_mut();
    }

    let c_str = unsafe { CStr::from_ptr(db_path_ptr) };
    let db_path = match c_str.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let spine = match CyboSpine::open(db_path) {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let data = match spine.list_shard_blastradius() {
        Ok(v) => v,
        Err(_) => return std::ptr::null_mut(),
    };

    let json = match serde_json::to_string(&data) {
        Ok(j) => j,
        Err(_) => return std::ptr::null_mut(),
    };

    let c_json = match CString::new(json) {
        Ok(c) => c,
        Err(_) => return std::ptr::null_mut(),
    };

    c_json.into_raw()
}

#[no_mangle]
pub extern "C" fn cybo_spine_summarize_workload_node_region_json(
    db_path_ptr: *const i8,
    node_id_ptr: *const i8,
    region_ptr: *const i8,
) -> *mut i8 {
    use std::ffi::{CStr, CString};

    if db_path_ptr.is_null() || node_id_ptr.is_null() || region_ptr.is_null() {
        return std::ptr::null_mut();
    }

    let db_path = unsafe { CStr::from_ptr(db_path_ptr) }
        .to_str()
        .unwrap_or_default();
    let node_id = unsafe { CStr::from_ptr(node_id_ptr) }
        .to_str()
        .unwrap_or_default();
    let region = unsafe { CStr::from_ptr(region_ptr) }
        .to_str()
        .unwrap_or_default();

    let spine = match CyboSpine::open(db_path) {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let summary_opt = match spine.summarize_workload_node_region(node_id, region) {
        Ok(opt) => opt,
        Err(_) => return std::ptr::null_mut(),
    };

    let json = match serde_json::to_string(&summary_opt) {
        Ok(j) => j,
        Err(_) => return std::ptr::null_mut(),
    };

    let c_json = match CString::new(json) {
        Ok(c) => c,
        Err(_) => return std::ptr::null_mut(),
    };

    c_json.into_raw()
}

/// Callers must free strings returned by *_json FFI with this helper.
#[no_mangle]
pub extern "C" fn cybo_spine_free_string(ptr: *mut i8) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        let _ = CString::from_raw(ptr);
    }
}
