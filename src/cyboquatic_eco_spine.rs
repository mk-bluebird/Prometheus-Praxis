// filename: src/cyboquatic_eco_spine.rs
// destination: eco_restoration_shard/src/cyboquatic_eco_spine.rs
//
// Purpose:
// - Non-actuating cdylib exposing read-only JSON APIs over the Cyboquatic
//   eco-metrics SQLite index and existing EcoNet blastradiuslink.
// - Designed for FFI from ALN, Lua, C++, and Kotlin/Android.
// - All functions are pure queries; no write or actuator bindings.

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::path::Path;

use rusqlite::{Connection, OpenFlags};
use serde::Serialize;

#[derive(Debug, Serialize)]
pub struct CyboMachineSummary {
    pub machine_id: i64,
    pub machinename: String,
    pub machinetype: String,
    pub nodeid: String,
    pub region: String,
    pub medium: String,
    pub lane: String,
    pub substrate_code: Option<String>,
    pub design_spechash: String,
}

#[derive(Debug, Serialize)]
pub struct CyboMachineEcoWindow {
    pub window_id: i64,
    pub machine_id: i64,
    pub window_start: String,
    pub window_end: String,
    pub kmetric: f64,
    pub emetric: f64,
    pub rmetric: f64,
    pub vt: f64,
    pub rcarbon: Option<f64>,
    pub rbiodiv: Option<f64>,
    pub rmaterials: Option<f64>,
    pub rdataquality: Option<f64>,
    pub eco_gain: f64,
    pub energy_kwh: f64,
    pub material_masskg: f64,
    pub biodeg_fraction: Option<f64>,
    pub non_toxicity: Option<f64>,
}

#[derive(Debug, Serialize)]
pub struct CyboMachineEcoPerJoule {
    pub machine_id: i64,
    pub machinename: String,
    pub machinetype: String,
    pub nodeid: String,
    pub region: String,
    pub medium: String,
    pub lane: String,
    pub window_start: String,
    pub window_end: String,
    pub kmetric: f64,
    pub emetric: f64,
    pub rmetric: f64,
    pub eco_gain: f64,
    pub energy_kwh: f64,
    pub eco_per_kwh: Option<f64>,
    pub material_masskg: f64,
    pub rcarbon: Option<f64>,
    pub rbiodiv: Option<f64>,
    pub rmaterials: Option<f64>,
    pub rdataquality: Option<f64>,
    pub vt: f64,
}

#[derive(Debug, Serialize)]
pub struct CyboMachineEcoRank {
    pub machine_id: i64,
    pub machinename: String,
    pub machinetype: String,
    pub nodeid: String,
    pub region: String,
    pub lane: String,
    pub kavg: f64,
    pub eavg: f64,
    pub ravg: f64,
    pub rcarbon_avg: f64,
    pub rbiodiv_avg: f64,
    pub rmaterials_avg: f64,
    pub rdataquality_avg: f64,
    pub vtavg: f64,
}

#[derive(Debug, Serialize)]
pub struct CyboMachineBlastRadius {
    pub machine_id: i64,
    pub machinename: String,
    pub nodeid: String,
    pub region: String,
    pub impacttype: String,
    pub impactscore: f64,
    pub vtsensitivity: Option<f64>,
    pub notes: Option<String>,
}

// Internal helpers

fn open_ro_db(path: &str) -> rusqlite::Result<Connection> {
    Connection::open_with_flags(
        Path::new(path),
        OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_NO_MUTEX,
    )
}

unsafe fn cstr_to_str(ptr: *const c_char) -> Result<&'static str, &'static str> {
    if ptr.is_null() {
        return Err("null pointer");
    }
    CStr::from_ptr(ptr)
        .to_str()
        .map_err(|_| "invalid UTF-8")
}

fn to_json_cstring<T: Serialize>(val: &T) -> *mut c_char {
    match serde_json::to_string(val) {
        Ok(json) => CString::new(json).map_or(std::ptr::null_mut(), |s| s.into_raw()),
        Err(_) => std::ptr::null_mut(),
    }
}

fn error_json(msg: &str) -> *mut c_char {
    #[derive(Serialize)]
    struct ErrorWrapper<'a> {
        error: &'a str,
    }
    let wrapped = ErrorWrapper { error: msg };
    to_json_cstring(&wrapped)
}

// Query helpers

fn query_cybo_machines(conn: &Connection, region: Option<&str>) -> rusqlite::Result<Vec<CyboMachineSummary>> {
    let mut out = Vec::new();
    if let Some(rgn) = region {
        let mut stmt = conn.prepare(
            r#"
            SELECT machine_id, machinename, machinetype, nodeid,
                   region, medium, lane, substrate_code, design_spechash
            FROM cybo_machine
            WHERE region = ?1
            ORDER BY nodeid, machinename
            "#,
        )?;
        let rows = stmt.query_map([rgn], |row| {
            Ok(CyboMachineSummary {
                machine_id: row.get(0)?,
                machinename: row.get(1)?,
                machinetype: row.get(2)?,
                nodeid: row.get(3)?,
                region: row.get(4)?,
                medium: row.get(5)?,
                lane: row.get(6)?,
                substrate_code: row.get(7)?,
                design_spechash: row.get(8)?,
            })
        })?;
        for r in rows {
            out.push(r?);
        }
    } else {
        let mut stmt = conn.prepare(
            r#"
            SELECT machine_id, machinename, machinetype, nodeid,
                   region, medium, lane, substrate_code, design_spechash
            FROM cybo_machine
            ORDER BY region, nodeid, machinename
            "#,
        )?;
        let rows = stmt.query_map([], |row| {
            Ok(CyboMachineSummary {
                machine_id: row.get(0)?,
                machinename: row.get(1)?,
                machinetype: row.get(2)?,
                nodeid: row.get(3)?,
                region: row.get(4)?,
                medium: row.get(5)?,
                lane: row.get(6)?,
                substrate_code: row.get(7)?,
                design_spechash: row.get(8)?,
            })
        })?;
        for r in rows {
            out.push(r?);
        }
    }
    Ok(out)
}

fn query_machine_windows(conn: &Connection, machine_id: i64) -> rusqlite::Result<Vec<CyboMachineEcoWindow>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT window_id, machine_id, window_start, window_end,
               kmetric, emetric, rmetric, vt,
               rcarbon, rbiodiv, rmaterials, rdataquality,
               eco_gain, energy_kwh, material_masskg,
               biodeg_fraction, non_toxicity
        FROM cybo_machine_window
        WHERE machine_id = ?1
        ORDER BY window_start
        "#,
    )?;
    let rows = stmt.query_map([machine_id], |row| {
        Ok(CyboMachineEcoWindow {
            window_id: row.get(0)?,
            machine_id: row.get(1)?,
            window_start: row.get(2)?,
            window_end: row.get(3)?,
            kmetric: row.get(4)?,
            emetric: row.get(5)?,
            rmetric: row.get(6)?,
            vt: row.get(7)?,
            rcarbon: row.get(8)?,
            rbiodiv: row.get(9)?,
            rmaterials: row.get(10)?,
            rdataquality: row.get(11)?,
            eco_gain: row.get(12)?,
            energy_kwh: row.get(13)?,
            material_masskg: row.get(14)?,
            biodeg_fraction: row.get(15)?,
            non_toxicity: row.get(16)?,
        })
    })?;
    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

fn query_ecoperjoule(conn: &Connection, nodeid: &str) -> rusqlite::Result<Vec<CyboMachineEcoPerJoule>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT machine_id, machinename, machinetype, nodeid,
               region, medium, lane,
               window_start, window_end,
               kmetric, emetric, rmetric,
               eco_gain, energy_kwh, eco_per_kwh,
               material_masskg,
               rcarbon, rbiodiv, rmaterials, rdataquality, vt
        FROM v_cybo_machine_ecoperjoule
        WHERE nodeid = ?1
        ORDER BY machinename, window_start
        "#,
    )?;
    let rows = stmt.query_map([nodeid], |row| {
        Ok(CyboMachineEcoPerJoule {
            machine_id: row.get(0)?,
            machinename: row.get(1)?,
            machinetype: row.get(2)?,
            nodeid: row.get(3)?,
            region: row.get(4)?,
            medium: row.get(5)?,
            lane: row.get(6)?,
            window_start: row.get(7)?,
            window_end: row.get(8)?,
            kmetric: row.get(9)?,
            emetric: row.get(10)?,
            rmetric: row.get(11)?,
            eco_gain: row.get(12)?,
            energy_kwh: row.get(13)?,
            eco_per_kwh: row.get(14)?,
            material_masskg: row.get(15)?,
            rcarbon: row.get(16)?,
            rbiodiv: row.get(17)?,
            rmaterials: row.get(18)?,
            rdataquality: row.get(19)?,
            vt: row.get(20)?,
        })
    })?;
    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

fn query_ecorank(conn: &Connection, region: &str) -> rusqlite::Result<Vec<CyboMachineEcoRank>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT machine_id, machinename, machinetype, nodeid,
               region, lane,
               kavg, eavg, ravg,
               rcarbon_avg, rbiodiv_avg, rmaterials_avg,
               rdataquality_avg, vtavg
        FROM v_cybo_machine_ecorank
        WHERE region = ?1
        ORDER BY lane, kavg DESC, eavg DESC, ravg ASC
        "#,
    )?;
    let rows = stmt.query_map([region], |row| {
        Ok(CyboMachineEcoRank {
            machine_id: row.get(0)?,
            machinename: row.get(1)?,
            machinetype: row.get(2)?,
            nodeid: row.get(3)?,
            region: row.get(4)?,
            lane: row.get(5)?,
            kavg: row.get(6)?,
            eavg: row.get(7)?,
            ravg: row.get(8)?,
            rcarbon_avg: row.get(9)?,
            rbiodiv_avg: row.get(10)?,
            rmaterials_avg: row.get(11)?,
            rdataquality_avg: row.get(12)?,
            vtavg: row.get(13)?,
        })
    })?;
    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

fn query_blastradius(conn: &Connection, machine_id: i64) -> rusqlite::Result<Vec<CyboMachineBlastRadius>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT machine_id, machinename, nodeid, region,
               impacttype, impactscore, vtsensitivity, notes
        FROM v_cybo_machine_blastradius
        WHERE machine_id = ?1
        ORDER BY impacttype
        "#,
    )?;
    let rows = stmt.query_map([machine_id], |row| {
        Ok(CyboMachineBlastRadius {
            machine_id: row.get(0)?,
            machinename: row.get(1)?,
            nodeid: row.get(2)?,
            region: row.get(3)?,
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

// C ABI: read-only JSON functions

#[no_mangle]
pub unsafe extern "C" fn cybo_get_machines_for_region(
    dbpath: *const c_char,
    region: *const c_char,
) -> *mut c_char {
    let db = match cstr_to_str(dbpath) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let region_opt = if region.is_null() {
        None
    } else {
        match cstr_to_str(region) {
            Ok(s) => Some(s),
            Err(m) => return error_json(m),
        }
    };
    let conn = match open_ro_db(db) {
        Ok(c) => c,
        Err(_) => return error_json("failed to open SQLite index"),
    };
    match query_cybo_machines(&conn, region_opt) {
        Ok(rows) => to_json_cstring(&rows),
        Err(_) => error_json("query failed"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn cybo_get_machine_windows(
    dbpath: *const c_char,
    machine_id: i64,
) -> *mut c_char {
    let db = match cstr_to_str(dbpath) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let conn = match open_ro_db(db) {
        Ok(c) => c,
        Err(_) => return error_json("failed to open SQLite index"),
    };
    match query_machine_windows(&conn, machine_id) {
        Ok(rows) => to_json_cstring(&rows),
        Err(_) => error_json("query failed"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn cybo_get_ecoperjoule_for_node(
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
    match query_ecoperjoule(&conn, node) {
        Ok(rows) => to_json_cstring(&rows),
        Err(_) => error_json("query failed"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn cybo_get_ecorank_for_region(
    dbpath: *const c_char,
    region: *const c_char,
) -> *mut c_char {
    let db = match cstr_to_str(dbpath) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let region = match cstr_to_str(region) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let conn = match open_ro_db(db) {
        Ok(c) => c,
        Err(_) => return error_json("failed to open SQLite index"),
    };
    match query_ecorank(&conn, region) {
        Ok(rows) => to_json_cstring(&rows),
        Err(_) => error_json("query failed"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn cybo_get_blastradius_for_machine(
    dbpath: *const c_char,
    machine_id: i64,
) -> *mut c_char {
    let db = match cstr_to_str(dbpath) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let conn = match open_ro_db(db) {
        Ok(c) => c,
        Err(_) => return error_json("failed to open SQLite index"),
    };
    match query_blastradius(&conn, machine_id) {
        Ok(rows) => to_json_cstring(&rows),
        Err(_) => error_json("query failed"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn cybo_free_json(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    let _ = CString::from_raw(ptr);
}
