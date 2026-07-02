// filename: crates/econet_blastradius_spine/src/lib.rs
// destination: EcoNet/crates/econet_blastradius_spine/src/lib.rs
// purpose:
//   Rust helper for loading db/blastradius_spine.sql into an in-memory SQLite
//   connection, and exposing safe read-only queries for Rust, Lua, C++, Kotlin.

// rust-version: 1.85, edition 2024, dual-licensed MIT OR Apache-2.0
#![forbid(unsafe_code)]

use rusqlite::{params, Connection, Error as SqlError, Row};
use serde::{Deserialize, Serialize};
use std::ffi::{CStr, CString};
use std::fs;
use std::os::raw::c_char;
use std::path::{Path, PathBuf};
use thiserror::Error;

#[derive(Debug)]
pub enum SpineError {
    Sql(SqlError),
    Io(std::io::Error),
    MissingSchema(String),
    Json(serde_json::Error),
}

impl From<SqlError> for SpineError {
    fn from(e: SqlError) -> Self {
        SpineError::Sql(e)
    }
}

impl From<std::io::Error> for SpineError {
    fn from(e: std::io::Error) -> Self {
        SpineError::Io(e)
    }
}

impl From<serde_json::Error> for SpineError {
    fn from(e: serde_json::Error) -> Self {
        SpineError::Json(e)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ShardEcoBlast {
    pub shard_id: i64,
    pub node_id: String,
    pub region: String,
    pub medium: String,
    pub lane: String,
    pub energy_eff_score: f64,
    pub carbon_score: f64,
    pub restoration_score: f64,
    pub k_factor: f64,
    pub e_factor: f64,
    pub r_factor: f64,
    pub t_start_utc: String,
    pub t_end_utc: String,
    pub radius_meters: Option<f64>,
    pub radius_hours: Option<f64>,
    pub hops: Option<i64>,
    pub propagation_type: Option<String>,
    pub blast_band: Option<String>,
    pub r_canal: Option<f64>,
    pub k_shard: f64,
    pub e_shard: f64,
    pub r_shard: f64,
    pub vt_shard: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ShardEcoImprovement {
    pub ecoscore_id: i64,
    pub shard_id: i64,
    pub lane: String,
    pub energy_eff_score: f64,
    pub carbon_score: f64,
    pub restoration_score: f64,
    pub delta_energy_eff: Option<f64>,
    pub delta_carbon_score: Option<f64>,
    pub delta_restoration_score: Option<f64>,
    pub t_start_utc: String,
    pub t_end_utc: String,
    pub policy_name: String,
    pub always_improve_ok: bool,
}

/// Consistent error type for the cross-spine kernel.
#[derive(Debug, Error)]
pub enum CrossSpineError {
    #[error("SQLite error: {0}")]
    Sqlite(#[from] rusqlite::Error),

    #[error("JSON error: {0}")]
    Json(#[from] serde_json::Error),

    #[error("Invalid argument: {0}")]
    InvalidArg(String),

    #[error("Missing data: {0}")]
    MissingData(String),
}

/// KER-weighted blast radius snapshot for a single machine or shard.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerBlastRadiusSnapshot {
    pub machine_id: String,
    pub region: String,
    pub lane: String,
    pub carbon_radius: f64,
    pub biodiversity_radius: f64,
    pub k_score: f64,
    pub e_score: f64,
    pub r_score: f64,
    pub vt_residual: f64,
    pub roh_scalar: f64,
    pub ker_weighted_carbon_radius: f64,
    pub ker_weighted_biodiversity_radius: f64,
}

/// Simple weighting rule: scale radii by K and Eco impact, damp by R and RoH.
fn ker_weight_radius(base_radius: f64, k: f64, e: f64, r: f64, roh: f64) -> f64 {
    if base_radius <= 0.0 {
        return 0.0;
    }
    let k_clamped = k.clamp(0.0, 1.0);
    let e_clamped = e.clamp(0.0, 1.0);
    let r_clamped = r.max(0.0);
    let roh_clamped = roh.max(0.0);

    let gain = (k_clamped * e_clamped).max(0.0);
    let penalty = 1.0 + r_clamped + roh_clamped;
    base_radius * gain / penalty
}

/// Internal helper: fetch raw carbon / biodiversity radii from Cyboquatic spine.
fn fetch_cyboquatic_radii(
    conn: &Connection,
    machine_id: &str,
) -> Result<(f64, f64, String, String), CrossSpineError> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            machineid,
            region,
            lane,
            MAX(CASE WHEN impactplane = 'CARBON' THEN impactscoresum ELSE 0.0 END) AS carbon_radius,
            MAX(CASE WHEN impactplane = 'BIODIVERSITY' THEN impactscoresum ELSE 0.0 END) AS biodiversity_radius
        FROM vmachineblastradius
        WHERE machineid = ?1
        GROUP BY machineid, region, lane
        "#,
    )?;

    let row = stmt.query_row([machine_id], |row| {
        let mid: String = row.get(0)?;
        let region: String = row.get(1)?;
        let lane: String = row.get(2)?;
        let carbon: f64 = row.get(3)?;
        let biodiv: f64 = row.get(4)?;
        Ok((carbon, biodiv, region, lane))
    })?;

    Ok(row)
}

/// Internal helper: fetch KER and Lyapunov / RoH scalars from EcoNet governance spine.
fn fetch_governance_ker(
    conn: &Connection,
    machine_id: &str,
) -> Result<(f64, f64, f64, f64, f64), CrossSpineError> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            kscore,
            escore,
            rscore,
            vt_residual,
            roh_scalar
        FROM vgov_machine_ker_window
        WHERE machineid = ?1
        ORDER BY window_end_utc DESC
        LIMIT 1
        "#,
    )?;

    let row = stmt.query_row([machine_id], |row| {
        let k: f64 = row.get(0)?;
        let e: f64 = row.get(1)?;
        let r: f64 = row.get(2)?;
        let vt: f64 = row.get(3)?;
        let roh: f64 = row.get(4)?;
        Ok((k, e, r, vt, roh))
    })?;

    Ok(row)
}

/// Core kernel: load both spines over a single SQLite DB and synthesize a snapshot.
pub fn blast_radius_kernel(
    db_path: &str,
    machine_id: &str,
) -> Result<KerBlastRadiusSnapshot, CrossSpineError> {
    if db_path.is_empty() {
        return Err(CrossSpineError::InvalidArg(
            "db_path must not be empty".to_string(),
        ));
    }
    if machine_id.is_empty() {
        return Err(CrossSpineError::InvalidArg(
            "machine_id must not be empty".to_string(),
        ));
    }

    let conn = Connection::open(db_path)?;

    let (carbon_radius, biodiversity_radius, region, lane) =
        fetch_cyboquatic_radii(&conn, machine_id)?;

    let (k_score, e_score, r_score, vt_residual, roh_scalar) =
        fetch_governance_ker(&conn, machine_id)?;

    let ker_weighted_carbon_radius =
        ker_weight_radius(carbon_radius, k_score, e_score, r_score, roh_scalar);

    let ker_weighted_biodiversity_radius =
        ker_weight_radius(biodiversity_radius, k_score, e_score, r_score, roh_scalar);

    Ok(KerBlastRadiusSnapshot {
        machine_id: machine_id.to_string(),
        region,
        lane,
        carbon_radius,
        biodiversity_radius,
        k_score,
        e_score,
        r_score,
        vt_residual,
        roh_scalar,
        ker_weighted_carbon_radius,
        ker_weighted_biodiversity_radius,
    })
}

/// Shard-level Eco blast and improvement spine over v_shard_* views.
pub struct BlastRadiusSpine {
    conn: Connection,
}

impl BlastRadiusSpine {
    /// Initialize an in-memory SQLite DB and load db/blastradius_spine.sql plus core schema.
    /// `root` should be the repo root where db/blastradius_spine.sql lives.
    pub fn new_in_memory(root: impl AsRef<Path>) -> Result<Self, SpineError> {
        let root = root.as_ref();
        let path = root.join("db").join("blastradius_spine.sql");
        if !path.exists() {
            return Err(SpineError::MissingSchema(
                path.to_string_lossy().to_string(),
            ));
        }
        let sql = fs::read_to_string(&path)?;
        let conn = Connection::open_in_memory()?;
        conn.execute_batch(&sql)?;
        Ok(Self { conn })
    }

    pub fn conn(&self) -> &Connection {
        &self.conn
    }

    pub fn conn_mut(&mut self) -> &mut Connection {
        &mut self.conn
    }

    pub fn list_eco_blast_for_region(
        &self,
        region: &str,
        min_restoration_score: f64,
    ) -> Result<Vec<ShardEcoBlast>, SpineError> {
        let mut stmt = self.conn.prepare(
            r#"
            SELECT
                shard_id,
                nodeid,
                region,
                medium,
                lane,
                energy_eff_score,
                carbon_score,
                restoration_score,
                k_factor,
                e_factor,
                r_factor,
                t_start_utc,
                t_end_utc,
                radius_meters,
                radius_hours,
                hops,
                propagation_type,
                blast_band,
                r_canal,
                k_shard,
                e_shard,
                r_shard,
                vt_shard
            FROM v_shard_eco_blast
            WHERE region = ?1
              AND restoration_score >= ?2
            "#,
        )?;
        let rows = stmt.query_map(params![region, min_restoration_score], |row| {
            Ok(row_to_eco_blast(row))
        })?;
        let mut out = Vec::new();
        for r in rows {
            out.push(r?);
        }
        Ok(out)
    }

    pub fn list_always_improve_ok(
        &self,
        lane: &str,
    ) -> Result<Vec<ShardEcoImprovement>, SpineError> {
        let mut stmt = self.conn.prepare(
            r#"
            SELECT
                ecoscore_id,
                shard_id,
                lane,
                energy_eff_score,
                carbon_score,
                restoration_score,
                delta_energy_eff,
                delta_carbon_score,
                delta_restoration_score,
                t_start_utc,
                t_end_utc,
                policy_name,
                always_improve_ok
            FROM v_shard_eco_improvement
            WHERE lane = ?1
              AND always_improve_ok = 1
            "#,
        )?;
        let rows = stmt.query_map(params![lane], |row| Ok(row_to_improvement(row)))?;
        let mut out = Vec::new();
        for r in rows {
            out.push(r?);
        }
        Ok(out)
    }

    pub fn to_json_eco_blast(
        &self,
        region: &str,
        min_restoration_score: f64,
    ) -> Result<String, SpineError> {
        let data = self.list_eco_blast_for_region(region, min_restoration_score)?;
        let json = serde_json::to_string_pretty(&data)?;
        Ok(json)
    }

    pub fn to_json_improvement(&self, lane: &str) -> Result<String, SpineError> {
        let data = self.list_always_improve_ok(lane)?;
        let json = serde_json::to_string_pretty(&data)?;
        Ok(json)
    }
}

fn row_to_eco_blast(row: &Row<'_>) -> ShardEcoBlast {
    ShardEcoBlast {
        shard_id: row.get(0).unwrap(),
        node_id: row.get(1).unwrap(),
        region: row.get(2).unwrap(),
        medium: row.get(3).unwrap(),
        lane: row.get(4).unwrap(),
        energy_eff_score: row.get(5).unwrap(),
        carbon_score: row.get(6).unwrap(),
        restoration_score: row.get(7).unwrap(),
        k_factor: row.get(8).unwrap(),
        e_factor: row.get(9).unwrap(),
        r_factor: row.get(10).unwrap(),
        t_start_utc: row.get(11).unwrap(),
        t_end_utc: row.get(12).unwrap(),
        radius_meters: row.get(13).ok(),
        radius_hours: row.get(14).ok(),
        hops: row.get(15).ok(),
        propagation_type: row.get(16).ok(),
        blast_band: row.get(17).ok(),
        r_canal: row.get(18).ok(),
        k_shard: row.get(19).unwrap(),
        e_shard: row.get(20).unwrap(),
        r_shard: row.get(21).unwrap(),
        vt_shard: row.get(22).unwrap(),
    }
}

fn row_to_improvement(row: &Row<'_>) -> ShardEcoImprovement {
    ShardEcoImprovement {
        ecoscore_id: row.get(0).unwrap(),
        shard_id: row.get(1).unwrap(),
        lane: row.get(2).unwrap(),
        energy_eff_score: row.get(3).unwrap(),
        carbon_score: row.get(4).unwrap(),
        restoration_score: row.get(5).unwrap(),
        delta_energy_eff: row.get(6).ok(),
        delta_carbon_score: row.get(7).ok(),
        delta_restoration_score: row.get(8).ok(),
        t_start_utc: row.get(9).unwrap(),
        t_end_utc: row.get(10).unwrap(),
        policy_name: row.get(11).unwrap(),
        always_improve_ok: {
            let v: i64 = row.get(12).unwrap();
            v != 0
        },
    }
}

/// C-FFI surface returning a JSON string for machine-level KER blast radius.
/// Non-actuating and suitable for AI-chat / external agents.
#[no_mangle]
pub extern "C" fn cyboquatic_blastradius_spine(
    db_path: *const c_char,
    machine_id: *const c_char,
) -> *mut c_char {
    if db_path.is_null() || machine_id.is_null() {
        return std::ptr::null_mut();
    }

    let db = match unsafe { CStr::from_ptr(db_path) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let mid = match unsafe { CStr::from_ptr(machine_id) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let snapshot = match blast_radius_kernel(db, mid) {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let json = match serde_json::to_string(&snapshot) {
        Ok(j) => j,
        Err(_) => return std::ptr::null_mut(),
    };

    match CString::new(json) {
        Ok(cstr) => cstr.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

/// Freeing function for JSON strings allocated by cyboquatic_blastradius_spine.
#[no_mangle]
pub extern "C" fn econet_governance_spine_free_string(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        let _ = CString::from_raw(ptr);
    }
}

/// C-FFI: initialize in-memory spine and return region Eco blast as JSON.
#[no_mangle]
pub extern "C" fn econet_blastradius_spine_init_json(
    root_path_utf8: *const c_char,
    region_utf8: *const c_char,
    min_restoration_score: f64,
) -> *mut c_char {
    if root_path_utf8.is_null() || region_utf8.is_null() {
        return std::ptr::null_mut();
    }
    let root_cstr = unsafe { CStr::from_ptr(root_path_utf8) };
    let region_cstr = unsafe { CStr::from_ptr(region_utf8) };
    let root = PathBuf::from(root_cstr.to_string_lossy().to_string());
    let region = region_cstr.to_string_lossy().to_string();

    let result = BlastRadiusSpine::new_in_memory(root)
        .and_then(|spine| spine.to_json_eco_blast(&region, min_restoration_score));

    match result {
        Ok(json) => match CString::new(json) {
            Ok(cstr) => cstr.into_raw(),
            Err(_) => std::ptr::null_mut(),
        },
        Err(_) => std::ptr::null_mut(),
    }
}

/// C-FFI: initialize in-memory spine and return lane improvements as JSON.
#[no_mangle]
pub extern "C" fn econet_blastradius_spine_improvement_json(
    root_path_utf8: *const c_char,
    lane_utf8: *const c_char,
) -> *mut c_char {
    if root_path_utf8.is_null() || lane_utf8.is_null() {
        return std::ptr::null_mut();
    }
    let root_cstr = unsafe { CStr::from_ptr(root_path_utf8) };
    let lane_cstr = unsafe { CStr::from_ptr(lane_utf8) };
    let root = PathBuf::from(root_cstr.to_string_lossy().to_string());
    let lane = lane_cstr.to_string_lossy().to_string();

    let result = BlastRadiusSpine::new_in_memory(root)
        .and_then(|spine| spine.to_json_improvement(&lane));

    match result {
        Ok(json) => match CString::new(json) {
            Ok(cstr) => cstr.into_raw(),
            Err(_) => std::ptr::null_mut(),
        },
        Err(_) => std::ptr::null_mut(),
    }
}

/// Free JSON strings allocated by econet_blastradius_spine_* FFI surfaces.
#[no_mangle]
pub extern "C" fn econet_blastradius_spine_free_string(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        let _ = CString::from_raw(ptr);
    }
}
