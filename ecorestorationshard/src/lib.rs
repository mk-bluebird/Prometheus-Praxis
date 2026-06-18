// filename: lib.rs
// destination: ecorestorationshard/src/lib.rs
// Purpose:
// - Non-actuating Rust cdylib exposing read-only JSON APIs over:
//   - EcoNet SQLite index (KER targets)
//   - Cyboquatic blast-radius diagnostics
//   - Cyboquatic workload trends
//   - Cyboquatic node eco-metrics view (vcybo_node_eco_metrics)
// - Designed as a shared spine for Lua, Kotlin/Android, C, and ALN within EcoNet.

#![crate_type = "cdylib"]

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::path::Path;

use rusqlite::{params, Connection, OpenFlags};
use serde::Serialize;

// ---------------------------
// Data structures
// ---------------------------

#[derive(Debug, Serialize)]
pub struct KerTargets {
    pub reponame: String,
    pub roleband: String,
    pub kertarget_k: f64,
    pub kertarget_e: f64,
    pub kertarget_r: f64,
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
    pub mean_vt_before: f64,
    pub mean_vt_after: f64,
    pub mean_r_carbon: Option<f64>,
    pub mean_r_biodiv: Option<f64>,
}

#[derive(Debug, Serialize)]
pub struct CyboNodeEcoMetrics {
    pub nodeid: String,
    pub displayname: String,
    pub region: String,
    pub medium: String,
    pub noderole: String,
    pub machineryclass: String,
    pub windowstartutc: Option<String>,
    pub windowendutc: Option<String>,
    pub totalrequestsj: Option<f64>,
    pub totalsurplusj: Option<f64>,
    pub acceptfraction: Option<f64>,
    pub mean_vt_before: Option<f64>,
    pub mean_vt_after: Option<f64>,
    pub mean_delta_vt: Option<f64>,
    pub mean_r_carbon: Option<f64>,
    pub mean_r_biodiv: Option<f64>,
    pub impacttype: Option<String>,
    pub impactscoresum: Option<f64>,
    pub vtsensitivitymean: Option<f64>,
    pub linkcount: Option<i64>,
}

#[derive(Debug, Serialize)]
struct ErrorEnvelope<'a> {
    error: &'a str,
}

// ---------------------------
// Internal helpers
// ---------------------------

fn open_ro_db(db_path: &str) -> rusqlite::Result<Connection> {
    let flags = OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_NO_MUTEX;
    Connection::open_with_flags(Path::new(db_path), flags)
}

unsafe fn cstr_to_str<'a>(ptr: *const c_char) -> Result<&'a str, &'static str> {
    if ptr.is_null() {
        return Err("null pointer");
    }
    CStr::from_ptr(ptr).to_str().map_err(|_| "invalid UTF-8")
}

fn to_json_c_string<T: Serialize>(val: &T) -> *mut c_char {
    match serde_json::to_string(val) {
        Ok(json) => match CString::new(json) {
            Ok(s) => s.into_raw(),
            Err(_) => std::ptr::null_mut(),
        },
        Err(_) => std::ptr::null_mut(),
    }
}

fn error_json(msg: &str) -> *mut c_char {
    let wrapped = ErrorEnvelope { error: msg };
    to_json_c_string(&wrapped)
}

// ---------------------------
// SQLite queries
// ---------------------------

fn query_ker_targets(conn: &Connection, reponame: &str) -> rusqlite::Result<KerTargets> {
    let mut stmt = conn.prepare(
        r#"
        SELECT reponame, roleband, kertargetk, kertargete, kertargetr
        FROM econetrepoindex
        WHERE reponame = ?1
        LIMIT 1
        "#,
    )?;
    stmt.query_row(params![reponame], |row| {
        Ok(KerTargets {
            reponame: row.get(0)?,
            roleband: row.get(1)?,
            kertarget_k: row.get(2)?,
            kertarget_e: row.get(3)?,
            kertarget_r: row.get(4)?,
        })
    })
}

fn query_blast_radius(conn: &Connection, nodeid: &str) -> rusqlite::Result<Vec<BlastRadiusEntry>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            sourcetype,
            sourceid,
            targettype,
            targetid,
            impacttype,
            impactscore,
            COALESCE(vtsensitivity, 0.0) AS vtsensitivity,
            COALESCE(notes, '')          AS notes
        FROM blastradiuslink
        WHERE sourcetype = 'NODE' AND sourceid = ?1
        ORDER BY impacttype, targettype, targetid
        "#,
    )?;

    let rows = stmt.query_map(params![nodeid], |row| {
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

fn query_workload_trends(
    conn: &Connection,
    nodeid: &str,
) -> rusqlite::Result<Vec<WorkloadTrendEntry>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            nodeid,
            channel,
            SUM(ereqj)      AS totalrequestsj,
            SUM(esurplusj)  AS totalsurplusj,
            AVG(vtbefore)   AS mean_vt_before,
            AVG(vtafter)    AS mean_vt_after,
            AVG(rcarbon)    AS mean_r_carbon,
            AVG(rbiodiv)    AS mean_r_biodiv
        FROM cyboworkloadledger
        WHERE nodeid = ?1
        GROUP BY nodeid, channel
        ORDER BY channel
        "#,
    )?;

    let rows = stmt.query_map(params![nodeid], |row| {
        Ok(WorkloadTrendEntry {
            nodeid: row.get(0)?,
            channel: row.get(1)?,
            totalrequestsj: row.get(2)?,
            totalsurplusj: row.get(3)?,
            mean_vt_before: row.get(4)?,
            mean_vt_after: row.get(5)?,
            mean_r_carbon: row.get(6)?,
            mean_r_biodiv: row.get(7)?,
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

fn query_cybo_node_eco_metrics(
    conn: &Connection,
    nodeid: &str,
) -> rusqlite::Result<Vec<CyboNodeEcoMetrics>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            nodeid,
            displayname,
            region,
            medium,
            noderole,
            machineryclass,
            windowstartutc,
            windowendutc,
            totalrequestsj,
            totalsurplusj,
            acceptfraction,
            meanvtbefore,
            meanvtafter,
            meandeltavt,
            meanrcarbon,
            meanrbiodiv,
            impacttype,
            impactscoresum,
            vtsensitivitymean,
            linkcount
        FROM vcybo_node_eco_metrics
        WHERE nodeid = ?1
        ORDER BY windowstartutc
        "#,
    )?;

    let rows = stmt.query_map(params![nodeid], |row| {
        Ok(CyboNodeEcoMetrics {
            nodeid: row.get(0)?,
            displayname: row.get(1)?,
            region: row.get(2)?,
            medium: row.get(3)?,
            noderole: row.get(4)?,
            machineryclass: row.get(5)?,
            windowstartutc: row.get(6)?,
            windowendutc: row.get(7)?,
            totalrequestsj: row.get(8)?,
            totalsurplusj: row.get(9)?,
            acceptfraction: row.get(10)?,
            mean_vt_before: row.get(11)?,
            mean_vt_after: row.get(12)?,
            mean_delta_vt: row.get(13)?,
            mean_r_carbon: row.get(14)?,
            mean_r_biodiv: row.get(15)?,
            impacttype: row.get(16)?,
            impactscoresum: row.get(17)?,
            vtsensitivitymean: row.get(18)?,
            linkcount: row.get(19)?,
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

// ---------------------------
// C ABI exports
// ---------------------------

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
        Ok(row) => to_json_c_string(&row),
        Err(_) => error_json("repo not found"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn econet_get_blast_radius_for_node(
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

    match query_blast_radius(&conn, node) {
        Ok(rows) => to_json_c_string(&rows),
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

    match query_workload_trends(&conn, node) {
        Ok(rows) => to_json_c_string(&rows),
        Err(_) => error_json("node not found or query failed"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn econet_get_cybo_node_eco_metrics(
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

    match query_cybo_node_eco_metrics(&conn, node) {
        Ok(rows) => to_json_c_string(&rows),
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
