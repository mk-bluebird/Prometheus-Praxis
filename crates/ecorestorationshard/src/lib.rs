// FILE: ecorestorationshard/src/lib.rs
// DESTINATION: ecorestorationshard/src/lib.rs
// REPO-TARGET: github.com/mk-bluebird/eco_restoration_shard

#![forbid(unsafe_code)]
#![warn(missing_docs)]

//! Ecorestorationshard top-level library.
//!
//! This crate provides a non-actuating index over EcoFort / EcoNet KER,
//! residual, EcoWealth, and governance artifacts. It is designed to
//! interact with the SQLite-based governance spine in read-only mode.
//!
//! It also exposes a cdylib-friendly, JSON-based FFI surface for Cyboquatic
//! diagnostics and EcoNet blast-radius, workload, and restoration windows.

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::path::Path;

use chrono::{DateTime, NaiveDateTime, Utc};
use rusqlite::{Connection, OpenFlags};
use serde::Serialize;
use thiserror::Error;

/// Errors produced by the Cyboquatic spine FFI surface.
#[derive(Debug, Error)]
pub enum SpineError {
    /// Invalid UTF-8 in incoming C string.
    #[error("invalid UTF-8 in C string")]
    InvalidUtf8,
    /// Database open failed.
    #[error("database open failed: {0}")]
    DbOpen(String),
    /// Query failed.
    #[error("query failed: {0}")]
    Query(String),
    /// Not found for the requested key.
    #[error("not found: {0}")]
    NotFound(String),
}

/// Repo-level KER targets for governance bands.
#[derive(Debug, Serialize)]
pub struct KerTargets {
    /// EcoNet repo name.
    pub reponame: String,
    /// Role band label (e.g. "RESEARCH", "PROD").
    pub roleband: String,
    /// Target K-axis score.
    pub kertargetk: f64,
    /// Target E-axis score.
    pub kertargete: f64,
    /// Target R-axis score.
    pub kertargetr: f64,
}

/// One blast-radius diagnostic entry.
#[derive(Debug, Serialize)]
pub struct BlastRadiusEntry {
    /// Source type (e.g. "NODE", "SHARD").
    pub sourcetype: String,
    /// Source identifier.
    pub sourceid: String,
    /// Target type (e.g. "REGION", "MACHINE").
    pub targettype: String,
    /// Target identifier.
    pub targetid: String,
    /// Impact type (e.g. "CARBON", "BIODIVERSITY").
    pub impacttype: String,
    /// Impact score in normalized corridor units.
    pub impactscore: f64,
    /// Approximate Lyapunov sensitivity across the blast surface.
    pub vtsensitivity: f64,
    /// Optional notes field.
    pub notes: String,
}

/// Aggregated workload metrics per node/channel.
#[derive(Debug, Serialize)]
pub struct WorkloadTrendEntry {
    /// Node identifier.
    pub nodeid: String,
    /// Logical channel label.
    pub channel: String,
    /// Total requested energy in joules.
    pub totalrequestsj: f64,
    /// Total surplus or returned energy in joules.
    pub totalsurplusj: f64,
    /// Mean Lyapunov residual before workloads.
    pub meanvtbefore: f64,
    /// Mean Lyapunov residual after workloads.
    pub meanvtafter: f64,
    /// Mean carbon risk scalar.
    pub meanrcarbon: f64,
    /// Mean biodiversity risk scalar.
    pub meanrbiodiv: f64,
}

/// Cyboquatic energy window entry (eco-per-joule, carbon-negative flag).
#[derive(Debug, Serialize)]
pub struct CyboEcoPlotEntry {
    /// Node identifier.
    pub nodeid: String,
    /// Basin identifier.
    pub basinid: String,
    /// Region code.
    pub region: String,
    /// Lane label.
    pub lane: String,
    /// Window start timestamp (ISO-8601).
    pub windowstartutc: String,
    /// Window end timestamp (ISO-8601).
    pub windowendutc: String,
    /// Total energy in joules over the window.
    pub energyjoules: f64,
    /// Eco impact per joule.
    pub ecoperjoule: f64,
    /// Carbon-negative guard flag.
    pub carbonnegativeok: bool,
    /// Lyapunov contribution over the window.
    pub vtcontrib: f64,
    /// K-axis score for the window.
    pub kscore: f64,
    /// E-axis score for the window.
    pub escore: f64,
    /// R-axis score for the window.
    pub rscore: f64,
}

/// Cyboquatic restoration window entry (radius, uplift, risk).
#[derive(Debug, Serialize)]
pub struct CyboRestorationEntry {
    /// Node identifier.
    pub nodeid: String,
    /// Window start timestamp (ISO-8601).
    pub windowstartutc: String,
    /// Window end timestamp (ISO-8601).
    pub windowendutc: String,
    /// Restoration radius in meters.
    pub restorationradiusm: f64,
    /// Restoration radius in hours.
    pub restorationradiushours: f64,
    /// Mass change over window in kilograms.
    pub deltamasswindowkg: f64,
    /// Eco-karma delta over window.
    pub deltakarmawindow: f64,
    /// Maximum governance risk scalar.
    pub gwriskmax: f64,
    /// Restoration guard flag.
    pub restorationok: bool,
}

/// Open a SQLite database strictly in read-only, non-mutex mode for the spine.
fn open_ro_db(db_path: &str) -> Result<Connection, SpineError> {
    Connection::open_with_flags(
        db_path,
        OpenFlags::SQLITE_OPEN_READONLY | OpenFlags::SQLITE_OPEN_NOMUTEX,
    )
    .map_err(|e| SpineError::DbOpen(e.to_string()))
}

/// Query econetrepoindex for KER targets of a given repo.
fn query_kertargets(conn: &Connection, reponame: &str) -> Result<KerTargets, SpineError> {
    let mut stmt = conn
        .prepare(
            r#"
            SELECT reponame, roleband, kertargetk, kertargete, kertargetr
            FROM econetrepoindex
            WHERE reponame = ?1
            LIMIT 1
            "#,
        )
        .map_err(|e| SpineError::Query(e.to_string()))?;

    let row = stmt
        .query_row([reponame], |r| {
            Ok(KerTargets {
                reponame: r.get(0)?,
                roleband: r.get(1)?,
                kertargetk: r.get(2)?,
                kertargete: r.get(3)?,
                kertargetr: r.get(4)?,
            })
        })
        .map_err(|e| SpineError::Query(e.to_string()))?;

    Ok(row)
}

/// Query blast-radius diagnostics for a given node.
fn query_blastradius(conn: &Connection, nodeid: &str) -> Result<Vec<BlastRadiusEntry>, SpineError> {
    let mut stmt = conn
        .prepare(
            r#"
            SELECT sourcetype, sourceid, targettype, targetid,
                   impacttype, impactscore,
                   COALESCE(vtsensitivity, 0.0),
                   COALESCE(notes, '')
            FROM blastradiuslink
            WHERE sourcetype = 'NODE'
              AND sourceid = ?1
            ORDER BY impacttype, targettype, targetid
            "#,
        )
        .map_err(|e| SpineError::Query(e.to_string()))?;

    let rows = stmt
        .query_map([nodeid], |r| {
            Ok(BlastRadiusEntry {
                sourcetype: r.get(0)?,
                sourceid: r.get(1)?,
                targettype: r.get(2)?,
                targetid: r.get(3)?,
                impacttype: r.get(4)?,
                impactscore: r.get(5)?,
                vtsensitivity: r.get(6)?,
                notes: r.get(7)?,
            })
        })
        .map_err(|e| SpineError::Query(e.to_string()))?;

    let mut out = Vec::new();
    for row in rows {
        out.push(row.map_err(|e| SpineError::Query(e.to_string()))?);
    }
    Ok(out)
}

/// Query workload trends per node/channel from cyboworkloadledger.
fn query_workloadtrends(conn: &Connection, nodeid: &str) -> Result<Vec<WorkloadTrendEntry>, SpineError> {
    let mut stmt = conn
        .prepare(
            r#"
            SELECT nodeid,
                   channel,
                   SUM(ereqj)      AS totalrequestsj,
                   SUM(esurplusj)  AS totalsurplusj,
                   AVG(vtbefore)   AS meanvtbefore,
                   AVG(vtafter)    AS meanvtafter,
                   AVG(rcarbon)    AS meanrcarbon,
                   AVG(rbiodiv)    AS meanrbiodiv
            FROM cyboworkloadledger
            WHERE nodeid = ?1
            GROUP BY nodeid, channel
            ORDER BY channel
            "#,
        )
        .map_err(|e| SpineError::Query(e.to_string()))?;

    let rows = stmt
        .query_map([nodeid], |r| {
            Ok(WorkloadTrendEntry {
                nodeid: r.get(0)?,
                channel: r.get(1)?,
                totalrequestsj: r.get(2)?,
                totalsurplusj: r.get(3)?,
                meanvtbefore: r.get(4)?,
                meanvtafter: r.get(5)?,
                meanrcarbon: r.get(6)?,
                meanrbiodiv: r.get(7)?,
            })
        })
        .map_err(|e| SpineError::Query(e.to_string()))?;

    let mut out = Vec::new();
    for row in rows {
        out.push(row.map_err(|e| SpineError::Query(e.to_string()))?);
    }
    Ok(out)
}

/// Query CyboquaticEcoPlot entries for a node (energy windows).
fn query_cybo_ecoplot(conn: &Connection, nodeid: &str) -> Result<Vec<CyboEcoPlotEntry>, SpineError> {
    let mut stmt = conn
        .prepare(
            r#"
            SELECT nodeid,
                   basinid,
                   region,
                   lane,
                   windowstartutc,
                   windowendutc,
                   energyjoules,
                   ecoperjoule,
                   carbonnegativeok,
                   vtcontrib,
                   kscore,
                   escore,
                   rscore
            FROM CyboquaticEcoPlot
            WHERE nodeid = ?1
            ORDER BY windowstartutc
            "#,
        )
        .map_err(|e| SpineError::Query(e.to_string()))?;

    let rows = stmt
        .query_map([nodeid], |r| {
            Ok(CyboEcoPlotEntry {
                nodeid: r.get(0)?,
                basinid: r.get(1)?,
                region: r.get(2)?,
                lane: r.get(3)?,
                windowstartutc: r.get::<_, String>(4)?,
                windowendutc: r.get::<_, String>(5)?,
                energyjoules: r.get(6)?,
                ecoperjoule: r.get(7)?,
                carbonnegativeok: {
                    let v: i64 = r.get(8)?;
                    v != 0
                },
                vtcontrib: r.get(9)?,
                kscore: r.get(10)?,
                escore: r.get(11)?,
                rscore: r.get(12)?,
            })
        })
        .map_err(|e| SpineError::Query(e.to_string()))?;

    let mut out = Vec::new();
    for row in rows {
        out.push(row.map_err(|e| SpineError::Query(e.to_string()))?);
    }
    Ok(out)
}

/// Query CyboquaticRestorationSurface entries for a node.
fn query_cybo_restoration(conn: &Connection, nodeid: &str) -> Result<Vec<CyboRestorationEntry>, SpineError> {
    let mut stmt = conn
        .prepare(
            r#"
            SELECT nodeid,
                   windowstartutc,
                   windowendutc,
                   restorationradiusm,
                   restorationradiushours,
                   deltamasswindowkg,
                   deltakarmawindow,
                   gwriskmax,
                   restorationok
            FROM CyboquaticRestorationSurface
            WHERE nodeid = ?1
            ORDER BY windowstartutc
            "#,
        )
        .map_err(|e| SpineError::Query(e.to_string()))?;

    let rows = stmt
        .query_map([nodeid], |r| {
            Ok(CyboRestorationEntry {
                nodeid: r.get(0)?,
                windowstartutc: r.get::<_, String>(1)?,
                windowendutc: r.get::<_, String>(2)?,
                restorationradiusm: r.get(3)?,
                restorationradiushours: r.get(4)?,
                deltamasswindowkg: r.get(5)?,
                deltakarmawindow: r.get(6)?,
                gwriskmax: r.get(7)?,
                restorationok: {
                    let v: i64 = r.get(8)?;
                    v != 0
                },
            })
        })
        .map_err(|e| SpineError::Query(e.to_string()))?;

    let mut out = Vec::new();
    for row in rows {
        out.push(row.map_err(|e| SpineError::Query(e.to_string()))?);
    }
    Ok(out)
}

/// Serialize a value to JSON and return it as a heap-allocated C string.
/// Caller must free via `econet_free_json`.
fn to_json_cstring<T: Serialize>(val: T) -> *mut c_char {
    match serde_json::to_string(&val) {
        Ok(s) => match CString::new(s) {
            Ok(c) => c.into_raw(),
            Err(_) => error_json_internal("serialization failed"),
        },
        Err(_) => error_json_internal("serialization failed"),
    }
}

/// Build an error JSON C string.
fn error_json_internal(msg: &str) -> *mut c_char {
    let payload = format!(r#"{{"error":"{}"}}"#, msg.replace('"', "'"));
    match CString::new(payload) {
        Ok(c) => c.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

/// Parse CStr safely from a C pointer.
fn cstr_to_str(ptr: *const c_char) -> Result<&'static str, SpineError> {
    if ptr.is_null() {
        return Err(SpineError::InvalidUtf8);
    }
    unsafe { CStr::from_ptr(ptr) }
        .to_str()
        .map_err(|_| SpineError::InvalidUtf8)
}

/// Public C ABI: get KER targets for a repo as JSON.
#[no_mangle]
pub extern "C" fn econet_get_kertargets(
    dbpath: *const c_char,
    reponame: *const c_char,
) -> *mut c_char {
    let dbpath_str = match cstr_to_str(dbpath) {
        Ok(s) => s,
        Err(e) => return error_json_internal(&e.to_string()),
    };
    let reponame_str = match cstr_to_str(reponame) {
        Ok(s) => s,
        Err(e) => return error_json_internal(&e.to_string()),
    };

    match open_ro_db(dbpath_str)
        .and_then(|conn| query_kertargets(&conn, reponame_str))
    {
        Ok(val) => to_json_cstring(val),
        Err(e) => error_json_internal(&e.to_string()),
    }
}

/// Public C ABI: get blast-radius diagnostics for a node as JSON.
#[no_mangle]
pub extern "C" fn econet_get_blastradius_for_node(
    dbpath: *const c_char,
    nodeid: *const c_char,
) -> *mut c_char {
    let dbpath_str = match cstr_to_str(dbpath) {
        Ok(s) => s,
        Err(e) => return error_json_internal(&e.to_string()),
    };
    let nodeid_str = match cstr_to_str(nodeid) {
        Ok(s) => s,
        Err(e) => return error_json_internal(&e.to_string()),
    };

    match open_ro_db(dbpath_str)
        .and_then(|conn| query_blastradius(&conn, nodeid_str))
    {
        Ok(val) => to_json_cstring(val),
        Err(e) => error_json_internal(&e.to_string()),
    }
}

/// Public C ABI: get workload trends for a node as JSON.
#[no_mangle]
pub extern "C" fn econet_get_workloadtrends_for_node(
    dbpath: *const c_char,
    nodeid: *const c_char,
) -> *mut c_char {
    let dbpath_str = match cstr_to_str(dbpath) {
        Ok(s) => s,
        Err(e) => return error_json_internal(&e.to_string()),
    };
    let nodeid_str = match cstr_to_str(nodeid) {
        Ok(s) => s,
        Err(e) => return error_json_internal(&e.to_string()),
    };

    match open_ro_db(dbpath_str)
        .and_then(|conn| query_workloadtrends(&conn, nodeid_str))
    {
        Ok(val) => to_json_cstring(val),
        Err(e) => error_json_internal(&e.to_string()),
    }
}

/// Public C ABI: get CyboquaticEcoPlot windows for a node as JSON.
#[no_mangle]
pub extern "C" fn econet_get_cybo_ecoplot_for_node(
    dbpath: *const c_char,
    nodeid: *const c_char,
) -> *mut c_char {
    let dbpath_str = match cstr_to_str(dbpath) {
        Ok(s) => s,
        Err(e) => return error_json_internal(&e.to_string()),
    };
    let nodeid_str = match cstr_to_str(nodeid) {
        Ok(s) => s,
        Err(e) => return error_json_internal(&e.to_string()),
    };

    match open_ro_db(dbpath_str)
        .and_then(|conn| query_cybo_ecoplot(&conn, nodeid_str))
    {
        Ok(val) => to_json_cstring(val),
        Err(e) => error_json_internal(&e.to_string()),
    }
}

/// Public C ABI: get CyboquaticRestorationSurface windows for a node as JSON.
#[no_mangle]
pub extern "C" fn econet_get_cybo_restoration_for_node(
    dbpath: *const c_char,
    nodeid: *const c_char,
) -> *mut c_char {
    let dbpath_str = match cstr_to_str(dbpath) {
        Ok(s) => s,
        Err(e) => return error_json_internal(&e.to_string()),
    };
    let nodeid_str = match cstr_to_str(nodeid) {
        Ok(s) => s,
        Err(e) => return error_json_internal(&e.to_string()),
    };

    match open_ro_db(dbpath_str)
        .and_then(|conn| query_cybo_restoration(&conn, nodeid_str))
    {
        Ok(val) => to_json_cstring(val),
        Err(e) => error_json_internal(&e.to_string()),
    }
}

/// Free a JSON buffer previously allocated by this library.
#[no_mangle]
pub extern "C" fn econet_free_json(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        let _ = CString::from_raw(ptr);
    }
}

/// Errors produced by the ecorestorationshard index.
#[derive(Debug, Error)]
pub enum ShardIndexError {
    /// Underlying SQLite error.
    #[error("SQLite error: {0}")]
    Sql(#[from] rusqlite::Error),

    /// Time parsing error.
    #[error("Time parse error: {0}")]
    TimeParse(#[from] chrono::ParseError),

    /// Generic configuration error.
    #[error("Configuration error: {0}")]
    Config(String),
}

/// Read-only handle to the EcoFort / EcoWealth governance database.
#[derive(Debug)]
pub struct ShardIndex {
    conn: Connection,
}

impl ShardIndex {
    /// Open a read-only connection to the governance database at `db_path`.
    ///
    /// The path must point to the canonical EcoFort spine DB containing
    /// `definitionregistry`, `planeweights`, `shardinstance`, and EcoWealth
    /// tables such as `StewardEcoWealthStatement`.
    pub fn open_readonly<P: AsRef<Path>>(db_path: P) -> Result<Self, ShardIndexError> {
        let conn = Connection::open_with_flags(
            db_path,
            OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_URI,
        )?;
        conn.pragma_update(None, "foreign_keys", "ON")?;
        Ok(Self { conn })
    }

    /// Return the number of active definitions in `definitionregistry`.
    ///
    /// This provides a quick health-check that the DB is wired and that
    /// the DefinitionRegistry has been seeded.
    pub fn active_definition_count(&self) -> Result<i64, ShardIndexError> {
        let mut stmt = self
            .conn
            .prepare("SELECT COUNT(*) FROM definitionregistry WHERE active = 1")?;
        let count: i64 = stmt.query_row([], |row| row.get(0))?;
        Ok(count)
    }

    /// A lightweight representation of a steward EcoWealth statement window.
    #[allow(clippy::struct_field_names)]
    pub fn list_steward_eco_wealth(
        &self,
        stewarddid: &str,
        limit: u32,
    ) -> Result<Vec<StewardEcoWealthRow>, ShardIndexError> {
        let sql = r#"
            SELECT
                stewarddid,
                region,
                lane,
                windowstartutc,
                windowendutc,
                kmean,
                emean,
                rmean,
                vtmaxwindow,
                ecounitfinal
            FROM StewardEcoWealthStatement
            WHERE stewarddid = ?
            ORDER BY windowendutc DESC
            LIMIT ?
        "#;

        let mut stmt = self.conn.prepare(sql)?;
        let mut rows = stmt.query((stewarddid, limit))?;
        let mut out = Vec::new();

        while let Some(row) = rows.next()? {
            let stewarddid: String = row.get(0)?;
            let region: String = row.get(1)?;
            let lane: String = row.get(2)?;
            let windowstartutc: String = row.get(3)?;
            let windowendutc: String = row.get(4)?;
            let kmean: f64 = row.get(5)?;
            let emean: f64 = row.get(6)?;
            let rmean: f64 = row.get(7)?;
            let vtmaxwindow: f64 = row.get(8)?;
            let ecounitfinal: f64 = row.get(9)?;

            out.push(StewardEcoWealthRow {
                stewarddid,
                region,
                lane,
                windowstartutc,
                windowendutc,
                kmean,
                emean,
                rmean,
                vtmaxwindow,
                ecounitfinal,
            });
        }

        Ok(out)
    }

    /// Cheap check that the EcoWealth statement window bounds are sane.
    ///
    /// Returns `true` if all rows have `windowstartutc <= windowendutc`.
    pub fn validate_eco_wealth_windows(&self) -> Result<bool, ShardIndexError> {
        let sql = r#"
            SELECT
                windowstartutc,
                windowendutc
            FROM StewardEcoWealthStatement
        "#;

        let mut stmt = self.conn.prepare(sql)?;
        let mut rows = stmt.query([])?;

        while let Some(row) = rows.next()? {
            let ws: String = row.get(0)?;
            let we: String = row.get(1)?;
            let ws_parsed: DateTime<Utc> = ws.parse()?;
            let we_parsed: DateTime<Utc> = we.parse()?;
            if we_parsed < ws_parsed {
                return Ok(false);
            }
        }

        Ok(true)
    }
}

/// Minimal view over a steward EcoWealth statement row.
///
/// This keeps the top-level crate focused on read-only analytics and
/// leaves detailed KER / residual math to the `kerresidual` crate.
#[derive(Debug, Clone)]
pub struct StewardEcoWealthRow {
    /// Steward DID (Bostrom address).
    pub stewarddid: String,
    /// Region code, e.g. "Phoenix-AZ-US".
    pub region: String,
    /// Lane label, e.g. "RESEARCH", "EXP", "PROD".
    pub lane: String,
    /// Window start timestamp in ISO-8601.
    pub windowstartutc: String,
    /// Window end timestamp in ISO-8601.
    pub windowendutc: String,
    /// Mean knowledge axis (K) over the window.
    pub kmean: f64,
    /// Mean ecological responsibility axis (E) over the window.
    pub emean: f64,
    /// Mean risk axis (R) over the window.
    pub rmean: f64,
    /// Maximum Lyapunov residual Vt over the window.
    pub vtmaxwindow: f64,
    /// Final EcoUnit scalar.
    pub ecounitfinal: f64,
}
