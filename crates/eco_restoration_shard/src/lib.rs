// filename: crates/eco_restoration_shard/src/lib.rs
// destination: eco_restoration_shard/crates/eco_restoration_shard/src/lib.rs
// target-repo: github.com/mk-bluebird/eco_restoration_shard
//
// Purpose
// - Provide a read-only Rust cdylib exposing JSON APIs over the EcoNet index
//   and the Cyboquatic blast-radius/ledger evidence for Lua and Kotlin/Android.

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::path::Path;

use rusqlite::{Connection, OpenFlags};
use serde::Serialize;

#[derive(Debug, Serialize)]
pub struct KerTargets {
    pub reponame: String,
    pub roleband: String,
    pub kertargetk: f64,
    pub kertargete: f64,
    pub kertargetr: f64,
}

#[derive(Debug, Serialize)]
pub struct BlastRadiusEntry {
    pub sourcetype: String,
    pub sourceid: String,
    pub targettype: String,
    pub targetid: String,
    pub impacttype: String,
    pub impactscore: f64,
    pub vtsensitivity: f64,
    pub notes: String,
}

#[derive(Debug, Serialize)]
pub struct WorkloadTrendEntry {
    pub nodeid: String,
    pub channel: String,
    pub totalrequestsj: f64,
    pub totalsurplusj: f64,
    pub meanvtbefore: f64,
    pub meanvtafter: f64,
    pub meanrcarbon: Option<f64>,
    pub meanrbiodiv: Option<f64>,
}

#[derive(Debug, Serialize)]
pub struct EnergyEfficiencyEntry {
    pub nodeid: String,
    pub surplus_fraction: f64,
    pub mean_vt_before: f64,
    pub mean_vt_after: f64,
    pub mean_delta_vt: f64,
}

fn open_ro_db(db_path: &str) -> rusqlite::Result<Connection> {
    Connection::open_with_flags(
        Path::new(db_path),
        OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_NO_MUTEX,
    )
}

unsafe fn cstr_to_str<'a>(ptr: *const c_char) -> Result<&'a str, &'static str> {
    if ptr.is_null() {
        return Err("null pointer");
    }
    CStr::from_ptr(ptr).to_str().map_err(|_| "invalid UTF-8")
}

fn to_json_cstring<T: Serialize>(val: T) -> *mut c_char {
    match serde_json::to_string(&val) {
        Ok(json) => match CString::new(json) {
            Ok(s) => s.into_raw(),
            Err(_) => std::ptr::null_mut(),
        },
        Err(_) => std::ptr::null_mut(),
    }
}

fn error_json(msg: &str) -> *mut c_char {
    let wrapped = serde_json::json!({ "error": msg });
    to_json_cstring(wrapped)
}

fn query_ker_targets(conn: &Connection, reponame: &str) -> rusqlite::Result<KerTargets> {
    let mut stmt = conn.prepare(
        r#"
        SELECT reponame, roleband, kertargetk, kertargete, kertargetr
        FROM   econetrepoindex
        WHERE  reponame = ?1
        LIMIT  1
        "#,
    )?;
    stmt.query_row([reponame], |row| {
        Ok(KerTargets {
            reponame: row.get(0)?,
            roleband: row.get(1)?,
            kertargetk: row.get(2)?,
            kertargete: row.get(3)?,
            kertargetr: row.get(4)?,
        })
    })
}

fn query_blast_radius_for_node(
    conn: &Connection,
    nodeid: &str,
) -> rusqlite::Result<Vec<BlastRadiusEntry>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT sourcetype,
               sourceid,
               targettype,
               targetid,
               impacttype,
               impactscore,
               COALESCE(vtsensitivity, 0.0),
               COALESCE(notes, '')
        FROM   blastradiuslink
        WHERE  sourcetype = 'NODE'
          AND  sourceid   = ?1
        ORDER BY impacttype, targettype, targetid
        "#,
    )?;

    let rows = stmt.query_map([nodeid], |row| {
        Ok(BlastRadiusEntry {
            sourcetype: row.get(0)?,
            sourceid: row.get(1)?,
            targettype: row.get(2)?,
            targetid: row.get(3)?,
            impacttype: row.get(4)?,
            impactscore: row.get(5)?,
            vtsensitivity: row.get(6)?,
            notes: row.get(7)?,
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

fn query_workload_trends_for_node(
    conn: &Connection,
    nodeid: &str,
) -> rusqlite::Result<Vec<WorkloadTrendEntry>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT nodeid,
               channel,
               SUM(ereqj)     AS totalrequestsj,
               SUM(esurplusj) AS totalsurplusj,
               AVG(vtbefore)  AS meanvtbefore,
               AVG(vtafter)   AS meanvtafter,
               AVG(rcarbon)   AS meanrcarbon,
               AVG(rbiodiv)   AS meanrbiodiv
        FROM   cyboworkloadledger
        WHERE  nodeid = ?1
        GROUP  BY nodeid, channel
        ORDER  BY channel
        "#,
    )?;

    let rows = stmt.query_map([nodeid], |row| {
        Ok(WorkloadTrendEntry {
            nodeid: row.get(0)?,
            channel: row.get(1)?,
            totalrequestsj: row.get(2)?,
            totalsurplusj: row.get(3)?,
            meanvtbefore: row.get(4)?,
            meanvtafter: row.get(5)?,
            meanrcarbon: row.get(6)?,
            meanrbiodiv: row.get(7)?,
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

fn query_energy_efficiency_for_node(
    conn: &Connection,
    nodeid: &str,
) -> rusqlite::Result<Vec<EnergyEfficiencyEntry>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT nodeid,
               total_requests_j,
               total_surplus_j,
               surplus_fraction,
               mean_vt_before,
               mean_vt_after,
               mean_delta_vt
        FROM   vcybo_energy_efficiency
        WHERE  nodeid = ?1
        "#,
    )?;

    let rows = stmt.query_map([nodeid], |row| {
        Ok(EnergyEfficiencyEntry {
            nodeid: row.get(0)?,
            surplus_fraction: row.get(3)?,
            mean_vt_before: row.get(4)?,
            mean_vt_after: row.get(5)?,
            mean_delta_vt: row.get(6)?,
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

#[no_mangle]
pub unsafe extern "C" fn econet_get_ker_targets(
    dbpath: *const c_char,
    reponame: *const c_char,
) -> *mut c_char {
    let db = match cstr_to_str(dbpath) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let repo = match cstr_to_str(reponame) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let conn = match open_ro_db(db) {
        Ok(c) => c,
        Err(_) => return error_json("failed to open SQLite index"),
    };
    match query_ker_targets(&conn, repo) {
        Ok(row) => to_json_cstring(row),
        Err(_) => error_json("repo not found"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn econet_get_blastradius_for_node(
    dbpath: *const c_char,
    nodeid: *const c_char,
) -> *mut c_char {
    let db = match cstr_to_str(dbpath) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let node = match cstr_to_str(nodeid) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let conn = match open_ro_db(db) {
        Ok(c) => c,
        Err(_) => return error_json("failed to open SQLite index"),
    };
    match query_blast_radius_for_node(&conn, node) {
        Ok(rows) => to_json_cstring(rows),
        Err(_) => error_json("node not found or query failed"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn econet_get_workload_trends_for_node(
    dbpath: *const c_char,
    nodeid: *const c_char,
) -> *mut c_char {
    let db = match cstr_to_str(dbpath) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let node = match cstr_to_str(nodeid) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let conn = match open_ro_db(db) {
        Ok(c) => c,
        Err(_) => return error_json("failed to open SQLite index"),
    };
    match query_workload_trends_for_node(&conn, node) {
        Ok(rows) => to_json_cstring(rows),
        Err(_) => error_json("node not found or query failed"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn econet_get_energy_efficiency_for_node(
    dbpath: *const c_char,
    nodeid: *const c_char,
) -> *mut c_char {
    let db = match cstr_to_str(dbpath) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let node = match cstr_to_str(nodeid) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let conn = match open_ro_db(db) {
        Ok(c) => c,
        Err(_) => return error_json("failed to open SQLite index"),
    };
    match query_energy_efficiency_for_node(&conn, node) {
        Ok(rows) => to_json_cstring(rows),
        Err(_) => error_json("node not found or query failed"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn econet_free_json(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    let _ = CString::from_raw(ptr);
}
