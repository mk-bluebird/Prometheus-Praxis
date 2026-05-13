// filename: crates/econet_blastradius_spine/src/lib.rs
// destination: EcoNet/crates/econet_blastradius_spine/src/lib.rs
// purpose:
//   Rust helper for loading db/blastradius_spine.sql into an in-memory SQLite
//   connection, and exposing safe read-only queries for Rust, Lua, C++, Kotlin.
//
// Cargo.toml (excerpt) for this crate:
//
// [package]
// name = "econet_blastradius_spine"
// version = "0.1.0"
// edition = "2021"
//
// [dependencies]
// rusqlite = { version = "0.31", features = ["bundled"] }
// serde = { version = "1.0", features = ["derive"] }
// serde_json = "1.0"
//
// [lib]
// name = "econet_blastradius_spine"
// crate-type = ["rlib", "cdylib"]

use rusqlite::{Connection, Error as SqlError, Row};
use serde::{Deserialize, Serialize};
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug)]
pub enum SpineError {
    Sql(SqlError),
    Io(std::io::Error),
    MissingSchema(String),
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

pub struct BlastRadiusSpine {
    conn: Connection,
}

impl BlastRadiusSpine {
    /// Initialize an in-memory SQLite DB and load blastradius_spine.sql plus core schema.
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
        let rows = stmt.query_map((region, min_restoration_score), |row| {
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
        let rows = stmt.query_map([lane], |row| Ok(row_to_improvement(row)))?;
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
        Ok(serde_json::to_string_pretty(&data).unwrap())
    }

    pub fn to_json_improvement(&self, lane: &str) -> Result<String, SpineError> {
        let data = self.list_always_improve_ok(lane)?;
        Ok(serde_json::to_string_pretty(&data).unwrap())
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

#[no_mangle]
pub extern "C" fn econet_blastradius_spine_init_json(
    root_path_utf8: *const i8,
    region_utf8: *const i8,
    min_restoration_score: f64,
) -> *mut i8 {
    use std::ffi::{CStr, CString};
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
        Ok(json) => CString::new(json).unwrap().into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn econet_blastradius_spine_improvement_json(
    root_path_utf8: *const i8,
    lane_utf8: *const i8,
) -> *mut i8 {
    use std::ffi::{CStr, CString};
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
        Ok(json) => CString::new(json).unwrap().into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn econet_blastradius_spine_free_string(ptr: *mut i8) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        let _ = CString::from_raw(ptr);
    }
}
