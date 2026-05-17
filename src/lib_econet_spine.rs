// filename: src/lib_econet_spine.rs
// destination: mk-bluebird/eco_restoration_shard/src/lib_econet_spine.rs
// role: non‑actuating Rust helper for SQLite blastradius/energy spine (FFI‑ready)

use rusqlite::{params, Connection, Result};
use std::path::Path;

#[derive(Debug, Clone)]
pub struct EcoShardSafestep {
    pub ledger_id: i64,
    pub shard_id: i64,
    pub node_id: String,
    pub channel: String,
    pub vt_before: f64,
    pub vt_after: f64,
    pub dv: f64,
    pub safestep_ok: bool,
}

#[derive(Debug, Clone)]
pub struct EcoNodeEnergyCarbon {
    pub node_id: String,
    pub n_events: i64,
    pub e_req_accept_j: f64,
    pub e_surplus_accept_j: f64,
    pub r_carbon_avg: Option<f64>,
    pub r_biodiv_avg: Option<f64>,
    pub dv_avg: Option<f64>,
}

#[derive(Debug, Clone)]
pub struct EcoCandidateEcorestorative {
    pub source_type: String,
    pub source_id: String,
    pub impact_carbon: f64,
    pub impact_biodiv: f64,
    pub vt_sensitivity_avg: f64,
    pub dv_avg: f64,
}

pub fn open_spine<P: AsRef<Path>>(path: P) -> Result<Connection> {
    let conn = Connection::open(path)?;
    conn.pragma_update(None, "foreign_keys", &"ON")?;
    Ok(conn)
}

pub fn query_safestep(conn: &Connection, shard_id: i64) -> Result<Vec<EcoShardSafestep>> {
    let mut stmt = conn.prepare(
        "SELECT ledger_id, shard_id, node_id, channel, vt_before, vt_after, dv, safestep_ok
         FROM v_shard_safestep
         WHERE shard_id = ?1",
    )?;
    let rows = stmt
        .query_map(params![shard_id], |row| {
            Ok(EcoShardSafestep {
                ledger_id: row.get(0)?,
                shard_id: row.get(1)?,
                node_id: row.get(2)?,
                channel: row.get(3)?,
                vt_before: row.get(4)?,
                vt_after: row.get(5)?,
                dv: row.get(6)?,
                safestep_ok: {
                    let v: i64 = row.get(7)?;
                    v == 1
                },
            })
        })?
        .collect::<Result<Vec<_>, _>>()?;
    Ok(rows)
}

pub fn query_best_nodes_for_energy_tailwind(conn: &Connection, limit: i64) -> Result<Vec<EcoNodeEnergyCarbon>> {
    let mut stmt = conn.prepare(
        "SELECT node_id, n_events, e_req_accept_j, e_surplus_accept_j,
                r_carbon_avg, r_biodiv_avg, dv_avg
         FROM v_node_energy_carbon
         WHERE n_events >= 5
           AND dv_avg <= 0.0
           AND e_surplus_accept_j >= e_req_accept_j
         ORDER BY r_carbon_avg ASC, e_req_accept_j ASC
         LIMIT ?1",
    )?;
    let rows = stmt
        .query_map(params![limit], |row| {
            Ok(EcoNodeEnergyCarbon {
                node_id: row.get(0)?,
                n_events: row.get(1)?,
                e_req_accept_j: row.get(2)?,
                e_surplus_accept_j: row.get(3)?,
                r_carbon_avg: row.get(4)?,
                r_biodiv_avg: row.get(5)?,
                dv_avg: row.get(6)?,
            })
        })?
        .collect::<Result<Vec<_>, _>>()?;
    Ok(rows)
}

pub fn query_candidate_ecorestorative(conn: &Connection, limit: i64) -> Result<Vec<EcoCandidateEcorestorative>> {
    let mut stmt = conn.prepare(
        "SELECT source_type, source_id, impact_carbon, impact_biodiv,
                vt_sensitivity_avg, dv_avg
         FROM v_candidate_ecorestorative
         ORDER BY impact_carbon DESC, impact_biodiv DESC
         LIMIT ?1",
    )?;
    let rows = stmt
        .query_map(params![limit], |row| {
            Ok(EcoCandidateEcorestorative {
                source_type: row.get(0)?,
                source_id: row.get(1)?,
                impact_carbon: row.get(2)?,
                impact_biodiv: row.get(3)?,
                vt_sensitivity_avg: row.get(4)?,
                dv_avg: row.get(5)?,
            })
        })?
        .collect::<Result<Vec<_>, _>>()?;
    Ok(rows)
}

// C‑FFI safe handles: expose only diagnostics, never actuators.

#[no_mangle]
pub extern "C" fn econet_spine_open(path_ptr: *const u8, len: usize) -> *mut Connection {
    if path_ptr.is_null() {
        return std::ptr::null_mut();
    }
    let slice = unsafe { std::slice::from_raw_parts(path_ptr, len) };
    let path_str = match std::str::from_utf8(slice) {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    match open_spine(path_str) {
        Ok(conn) => Box::into_raw(Box::new(conn)),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn econet_spine_close(conn_ptr: *mut Connection) {
    if conn_ptr.is_null() {
        return;
    }
    unsafe { Box::from_raw(conn_ptr); }
}
