// filename: lib.rs
// destination: ecorestorationshard/src/lib.rs
// Purpose:
// - Provide a cdylib exposing read-only JSON APIs over the EcoNet index,
//   Cyboquatic blast-radius ledger, workload trends, and cybo_node eco-metrics
//   for Lua and Kotlin/Android.
// - Strictly non-actuating; all queries are read-only.

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::path::Path;

use rusqlite::{Connection, OpenFlags};
use serde::Serialize;

// ----------------------------
// Core structs (existing)
// ----------------------------

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

// ----------------------------
// New: Cyboquatic eco-metrics
// ----------------------------

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
    pub meanvtbefore: Option<f64>,
    pub meanvtafter: Option<f64>,
    pub meandeltavt: Option<f64>,
    pub meanrcarbon: Option<f64>,
    pub meanrbiodiv: Option<f64>,
    pub impacttype: Option<String>,
    pub impactscoresum: Option<f64>,
    pub vtsensitivitymean: Option<f64>,
    pub linkcount: Option<i64>,
}

// ----------------------------
// Internal helpers
// ----------------------------

fn open_ro_db(dbpath: &str) -> rusqlite::Result<Connection> {
    Connection::open_with_flags(
        Path::new(dbpath),
        OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_NO_MUTEX,
    )
}

// Convert C string pointer to Rust &str safely.
unsafe fn cstr_to_str<'a>(ptr: *const c_char) -> Result<&'a str, &'static str> {
    if ptr.is_null() {
        return Err("null pointer");
    }
    CStr::from_ptr(ptr).to_str().map_err(|_| "invalid UTF-8")
}

// Serialize a value to JSON and hand it out as a C string pointer.
fn to_json_cstring<T: Serialize>(val: T) -> *mut c_char {
    match serde_json::to_string(&val) {
        Ok(json) => CString::new(json).map(|s| s.into_raw()).unwrap_or(std::ptr::null_mut()),
        Err(_) => std::ptr::null_mut(),
    }
}

// Serialize an error message to JSON envelope.
fn error_json(msg: &str) -> *mut c_char {
    #[derive(Serialize)]
    struct ErrorEnvelope<'a> {
        error: &'a str,
    }
    to_json_cstring(ErrorEnvelope { error: msg })
}

// ----------------------------
// Query functions (existing)
// ----------------------------

fn query_ker_targets(conn: &Connection, reponame: &str) -> rusqlite::Result<KerTargets> {
    let mut stmt = conn.prepare(
        r#"
        SELECT reponame, roleband, kertargetk, kertargete, kertargetr
        FROM econetrepoindex
        WHERE reponame = ?1
        LIMIT 1
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

fn query_blast_radius(conn: &Connection, nodeid: &str) -> rusqlite::Result<Vec<BlastRadiusEntry>> {
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
        FROM blastradiuslink
        WHERE sourcetype = 'NODE'
          AND sourceid = ?1
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

fn query_workload_trends(
    conn: &Connection,
    nodeid: &str,
) -> rusqlite::Result<Vec<WorkloadTrendEntry>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT nodeid,
               channel,
               SUM(ereqj) AS totalrequestsj,
               SUM(esurplusj) AS totalsurplusj,
               AVG(vtbefore) AS meanvtbefore,
               AVG(vtafter)  AS meanvtafter,
               AVG(rcarbon)  AS meanrcarbon,
               AVG(rbiodiv)  AS meanrbiodiv
        FROM cyboworkloadledger
        WHERE nodeid = ?1
        GROUP BY nodeid, channel
        ORDER BY channel
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

// ----------------------------
// New: Cyboquatic eco-metrics query
// ----------------------------

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

    let rows = stmt.query_map([nodeid], |row| {
        Ok(CyboNodeEcoMetrics {
            nodeid: row.get(0)?,
            displayname: row.get(1)?,
            region: row.get(2)?,
            medium: row.get(3)?,
            noderole: row.get(4)?,
            machineryclass: row.get(5)?,
            windowstartutc: row.get::<_, Option<String>>(6)?,
            windowendutc: row.get::<_, Option<String>>(7)?,
            totalrequestsj: row.get::<_, Option<f64>>(8)?,
            totalsurplusj: row.get::<_, Option<f64>>(9)?,
            acceptfraction: row.get::<_, Option<f64>>(10)?,
            meanvtbefore: row.get::<_, Option<f64>>(11)?,
            meanvtafter: row.get::<_, Option<f64>>(12)?,
            meandeltavt: row.get::<_, Option<f64>>(13)?,
            meanrcarbon: row.get::<_, Option<f64>>(14)?,
            meanrbiodiv: row.get::<_, Option<f64>>(15)?,
            impacttype: row.get::<_, Option<String>>(16)?,
            impactscoresum: row.get::<_, Option<f64>>(17)?,
            vtsensitivitymean: row.get::<_, Option<f64>>(18)?,
            linkcount: row.get::<_, Option<i64>>(19)?,
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

// ----------------------------
// C ABI exports
// ----------------------------

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
    match query_workload_trends(&conn, node) {
        Ok(rows) => to_json_cstring(rows),
        Err(_) => error_json("node not found or query failed"),
    }
}

// New: Cyboquatic eco-metrics JSON for a node.
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
        Ok(rows) => to_json_cstring(rows),
        Err(_) => error_json("node not found or query failed"),
    }
}

// Free JSON strings returned by this library.
// Safety: ptr must be a pointer previously returned by one of the econet_* functions.
#[no_mangle]
pub unsafe extern "C" fn econet_free_json(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    let _ = CString::from_raw(ptr);
}
