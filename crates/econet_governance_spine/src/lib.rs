// Filename: crates/econet_governance_spine/src/lib.rs
// Destination: crates/econet_governance_spine/src/lib.rs

#![forbid(unsafe_code)]

#[cfg(test)]
#![allow(unsafe_code)]

use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::path::PathBuf;
use std::ptr;

use thiserror::Error;

use rusqlite::{params, Connection, OpenFlags, OptionalExtension, Row};
use serde::Serialize;
use serde_json::json;

mod guards;
mod schema;

pub use guards::{
    KerGuardInputs, KerGuardResult, KerUpgradeGuard, LaneFilter, LaneGuardInputs,
    LaneGuardResult, Mt6883GuardInputs, Mt6883GuardResult,
};
pub use schema::{ExpectedSchema, ExpectedTable, SchemaVerifier};

#[derive(Debug, Error)]
pub enum SpineError {
    #[error("SQLite error: {0}")]
    Sql(#[from] rusqlite::Error),

    #[error("Schema mismatch: {0}")]
    SchemaMismatch(String),

    #[error("Missing definition registry entry for id '{0}'")]
    MissingDefinitionRegistry(String),

    #[error("Missing lane status for shard '{0}'")]
    MissingLaneStatus(String),

    #[error("Missing KER residual for shard '{0}'")]
    MissingKerResidual(String),

    #[error("Missing plane weights for shard '{0}'")]
    MissingPlaneWeights(String),

    #[error("Missing blast radius for shard '{0}'")]
    MissingBlastRadius(String),

    #[error("Missing eco wealth statement for steward '{0}'")]
    MissingEcoWealth(String),

    #[error("Missing cyboquatic metrics for node '{0}'")]
    MissingCyboquaticMetrics(String),

    #[error("UTF-8 conversion error")]
    Utf8,

    #[error("JSON serialization error: {0}")]
    Json(#[from] serde_json::Error),

    #[error("Invalid argument: {0}")]
    InvalidArgument(String),
}

#[derive(Debug, Clone)]
pub struct KerResidual {
    pub shard_id: String,
    pub region: String,
    pub k: f64,
    pub e: f64,
    pub r: f64,
    pub vt: f64,
}

#[derive(Debug, Clone)]
pub struct PlaneWeight {
    pub plane_name: String,
    pub weight: f64,
    pub non_offsettable: bool,
}

#[derive(Debug, Clone)]
pub struct BlastRadius {
    pub shard_id: String,
    pub radius_meters: f64,
    pub adjacency_count: i64,
}

#[derive(Debug, Clone)]
pub struct LaneStatus {
    pub shard_id: String,
    pub region: String,
    pub lane: String,
    pub verdict: String,
    pub ker_k: f64,
    pub ker_e: f64,
    pub ker_r: f64,
    pub residual_vt: f64,
    pub max_staleness_hours: i64,
    pub expires_utc: i64,
    pub carbon_negative_ok: bool,
    pub restoration_ok: bool,
}

#[derive(Debug, Clone)]
pub struct DefinitionRegistryEntry {
    pub def_id: String,
    pub logical_name: String,
    pub repo_path: String,
    pub category: String,
    pub frozen: bool,
}

#[derive(Debug, Clone)]
pub struct EcoWealthStatement {
    pub steward_did: String,
    pub shard_id: String,
    pub wealth_score: f64,
    pub k_effective: f64,
    pub e_effective: f64,
    pub r_effective: f64,
}

#[derive(Debug, Clone)]
pub struct CyboquaticMetrics {
    pub node_id: String,
    pub shard_id: String,
    pub eco_per_joule: f64,
    pub carbon_negative_ok: bool,
    pub restoration_ok: bool,
}

#[derive(Debug)]
pub struct GovernanceSpine {
    conn: Connection,
    expected_schema: ExpectedSchema,
}

impl GovernanceSpine {
    pub fn open(db_path: &PathBuf, expected_schema: ExpectedSchema) -> Result<Self, SpineError> {
        let flags = OpenFlags::SQLITE_OPEN_READONLY | OpenFlags::SQLITE_OPEN_NOMUTEX;
        let conn = Connection::open_with_flags(db_path, flags)?;
        let spine = GovernanceSpine { conn, expected_schema };
        spine.verify_schema()?;
        Ok(spine)
    }

    pub fn verify_schema(&self) -> Result<(), SpineError> {
        let verifier = SchemaVerifier::new(&self.conn, self.expected_schema.clone());
        verifier.verify().map_err(SpineError::SchemaMismatch)
    }

    pub fn get_ker_residual(&self, shard_id: &str) -> Result<KerResidual, SpineError> {
        let mut stmt = self.conn.prepare(
            "SELECT shard_id,
                    region,
                    ker_k,
                    ker_e,
                    ker_r,
                    residual_vt
             FROM vresidualkernel
             WHERE shard_id = ?1",
        )?;
        let opt = stmt
            .query_row(params![shard_id], |row| Self::map_ker_residual(row))
            .optional()?;
        match opt {
            Some(r) => Ok(r),
            None => Err(SpineError::MissingKerResidual(shard_id.to_string())),
        }
    }

    pub fn get_plane_weights(&self, shard_id: &str) -> Result<Vec<PlaneWeight>, SpineError> {
        let mut stmt = self.conn.prepare(
            "SELECT plane_name,
                    weight,
                    non_offsettable
             FROM vplaneweights
             WHERE shard_id = ?1",
        )?;
        let iter = stmt.query_map(params![shard_id], |row| Self::map_plane_weight(row))?;
        let mut result = Vec::new();
        for item in iter {
            result.push(item?);
        }
        if result.is_empty() {
            Err(SpineError::MissingPlaneWeights(shard_id.to_string()))
        } else {
            Ok(result)
        }
    }

    pub fn get_blast_radius(&self, shard_id: &str) -> Result<BlastRadius, SpineError> {
        let mut stmt = self.conn.prepare(
            "SELECT shard_id,
                    radius_meters,
                    adjacency_count
             FROM vshardblast
             WHERE shard_id = ?1",
        )?;
        let opt = stmt
            .query_row(params![shard_id], |row| Self::map_blast_radius(row))
            .optional()?;
        match opt {
            Some(b) => Ok(b),
            None => Err(SpineError::MissingBlastRadius(shard_id.to_string())),
        }
    }

    pub fn get_lane_status(&self, shard_id: &str) -> Result<LaneStatus, SpineError> {
        let mut stmt = self.conn.prepare(
            "SELECT shard_id,
                    region,
                    lane,
                    verdict,
                    ker_k,
                    ker_e,
                    ker_r,
                    residual_vt,
                    max_staleness_hours,
                    expires_utc,
                    carbon_negative_ok,
                    restoration_ok
             FROM vlaneadmissibility
             WHERE shard_id = ?1",
        )?;
        let opt = stmt
            .query_row(params![shard_id], |row| Self::map_lane_status(row))
            .optional()?;
        match opt {
            Some(l) => Ok(l),
            None => Err(SpineError::MissingLaneStatus(shard_id.to_string())),
        }
    }

    pub fn get_definition_registry_entry(
        &self,
        def_id: &str,
    ) -> Result<DefinitionRegistryEntry, SpineError> {
        let mut stmt = self.conn.prepare(
            "SELECT defid,
                    logicalname,
                    repopath,
                    category,
                    frozen
             FROM definitionregistry
             WHERE defid = ?1",
        )?;
        let opt = stmt
            .query_row(params![def_id], |row| Self::map_definition_registry(row))
            .optional()?;
        match opt {
            Some(e) => Ok(e),
            None => Err(SpineError::MissingDefinitionRegistry(def_id.to_string())),
        }
    }

    pub fn get_eco_wealth_latest(
        &self,
        steward_did: &str,
    ) -> Result<EcoWealthStatement, SpineError> {
        let mut stmt = self.conn.prepare(
            "SELECT steward_did,
                    shard_id,
                    wealth_score,
                    k_effective,
                    e_effective,
                    r_effective
             FROM vsteward_eco_wealth_latest
             WHERE steward_did = ?1",
        )?;
        let opt = stmt
            .query_row(params![steward_did], |row| Self::map_eco_wealth(row))
            .optional()?;
        match opt {
            Some(e) => Ok(e),
            None => Err(SpineError::MissingEcoWealth(steward_did.to_string())),
        }
    }

    pub fn get_cyboquatic_metrics(
        &self,
        node_id: &str,
    ) -> Result<CyboquaticMetrics, SpineError> {
        let mut stmt = self.conn.prepare(
            "SELECT node_id,
                    shard_id,
                    eco_per_joule,
                    carbonnegativeok,
                    restorationok
             FROM vcyboquaticecoperjoule
             WHERE node_id = ?1",
        )?;
        let opt = stmt
            .query_row(params![node_id], |row| Self::map_cyboquatic(row))
            .optional()?;
        match opt {
            Some(m) => Ok(m),
            None => Err(SpineError::MissingCyboquaticMetrics(node_id.to_string())),
        }
    }

    fn map_ker_residual(row: &Row) -> Result<KerResidual, rusqlite::Error> {
        Ok(KerResidual {
            shard_id: row.get(0)?,
            region: row.get(1)?,
            k: row.get(2)?,
            e: row.get(3)?,
            r: row.get(4)?,
            vt: row.get(5)?,
        })
    }

    fn map_plane_weight(row: &Row) -> Result<PlaneWeight, rusqlite::Error> {
        Ok(PlaneWeight {
            plane_name: row.get(0)?,
            weight: row.get(1)?,
            non_offsettable: row.get::<_, i64>(2)? != 0,
        })
    }

    fn map_blast_radius(row: &Row) -> Result<BlastRadius, rusqlite::Error> {
        Ok(BlastRadius {
            shard_id: row.get(0)?,
            radius_meters: row.get(1)?,
            adjacency_count: row.get(2)?,
        })
    }

    fn map_lane_status(row: &Row) -> Result<LaneStatus, rusqlite::Error> {
        Ok(LaneStatus {
            shard_id: row.get(0)?,
            region: row.get(1)?,
            lane: row.get(2)?,
            verdict: row.get(3)?,
            ker_k: row.get(4)?,
            ker_e: row.get(5)?,
            ker_r: row.get(6)?,
            residual_vt: row.get(7)?,
            max_staleness_hours: row.get(8)?,
            expires_utc: row.get(9)?,
            carbon_negative_ok: row.get::<_, i64>(10)? != 0,
            restoration_ok: row.get::<_, i64>(11)? != 0,
        })
    }

    fn map_definition_registry(row: &Row) -> Result<DefinitionRegistryEntry, rusqlite::Error> {
        Ok(DefinitionRegistryEntry {
            def_id: row.get(0)?,
            logical_name: row.get(1)?,
            repo_path: row.get(2)?,
            category: row.get(3)?,
            frozen: row.get::<_, i64>(4)? != 0,
        })
    }

    fn map_eco_wealth(row: &Row) -> Result<EcoWealthStatement, rusqlite::Error> {
        Ok(EcoWealthStatement {
            steward_did: row.get(0)?,
            shard_id: row.get(1)?,
            wealth_score: row.get(2)?,
            k_effective: row.get(3)?,
            e_effective: row.get(4)?,
            r_effective: row.get(5)?,
        })
    }

    fn map_cyboquatic(row: &Row) -> Result<CyboquaticMetrics, rusqlite::Error> {
        Ok(CyboquaticMetrics {
            node_id: row.get(0)?,
            shard_id: row.get(1)?,
            eco_per_joule: row.get(2)?,
            carbon_negative_ok: row.get::<_, i64>(3)? != 0,
            restoration_ok: row.get::<_, i64>(4)? != 0,
        })
    }
}

pub fn load_expected_schema() -> ExpectedSchema {
    let mut tables = HashMap::new();
    tables.insert(
        "lanestatusshard".to_string(),
        ExpectedTable::lanestatusshard(),
    );
    tables.insert(
        "definitionregistry".to_string(),
        ExpectedTable::definitionregistry(),
    );
    tables.insert(
        "ecounitshard2026v1".to_string(),
        ExpectedTable::ecounitshard2026v1(),
    );
    tables.insert(
        "stewardecowealthstatement".to_string(),
        ExpectedTable::stewardecowealthstatement(),
    );
    tables.insert(
        "kerresidual".to_string(),
        ExpectedTable::kerresidual(),
    );
    // AI-facing views
    tables.insert(
        "vresidualkernel".to_string(),
        ExpectedTable::vresidualkernel(),
    );
    tables.insert(
        "vshardblast".to_string(),
        ExpectedTable::vshardblast(),
    );
    tables.insert(
        "vlaneadmissibility".to_string(),
        ExpectedTable::vlaneadmissibility(),
    );
    tables.insert(
        "vcyboquaticecoperjoule".to_string(),
        ExpectedTable::vcyboquaticecoperjoule(),
    );
    tables.insert(
        "veconet_repo_manifest_agent".to_string(),
        ExpectedTable::veconet_repo_manifest_agent(),
    );
    tables.insert(
        "vagentsafecatalog".to_string(),
        ExpectedTable::vagentsafecatalog(),
    );

    ExpectedSchema { tables }
}

// --- FFI + AI context surface -----------------------------------------------

#[repr(C)]
pub struct ShardIndex {
    conn: Connection,
}

impl ShardIndex {
    fn new(db_path: &str) -> Result<Self, SpineError> {
        let flags = OpenFlags::SQLITE_OPEN_READONLY | OpenFlags::SQLITE_OPEN_NOMUTEX;
        let conn = Connection::open_with_flags(db_path, flags)?;
        Ok(ShardIndex { conn })
    }
}

fn cstr_to_str(ptr: *const c_char) -> Result<&'static str, SpineError> {
    if ptr.is_null() {
        return Err(SpineError::Utf8);
    }
    let cstr = unsafe { CStr::from_ptr(ptr) };
    cstr.to_str().map_err(|_| SpineError::Utf8)
}

fn to_json_cstring<T>(value: &T) -> *mut c_char
where
    T: Serialize,
{
    match serde_json::to_string(value) {
        Ok(json_str) => match CString::new(json_str) {
            Ok(cstr) => cstr.into_raw(),
            Err(_) => ptr::null_mut(),
        },
        Err(_) => ptr::null_mut(),
    }
}

fn error_json_internal(msg: &str) -> *mut c_char {
    let payload = json!({ "error": msg }).to_string();
    match CString::new(payload) {
        Ok(cstr) => cstr.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

macro_rules! impl_ffi_query {
    (
        $(#[$attr:meta])*
        pub extern "C" fn $name:ident ( $($arg_name:ident : $arg_type:ty),* ) -> *mut c_char
        $body:block
    ) => {
        $(#[$attr])*
        pub extern "C" fn $name($($arg_name : $arg_type),*) -> *mut c_char {
            $(
                if $arg_name.is_null() {
                    return error_json_internal("Invalid null pointer provided to function");
                }
            )*

            let result: Result<_, SpineError> = { $body };

            match result {
                Ok(value) => to_json_cstring(&value),
                Err(e) => error_json_internal(&e.to_string()),
            }
        }
    };
}

#[derive(Debug, Serialize)]
pub struct RepoManifest {
    pub reponame: String,
    pub roleband: String,
    pub lane: String,
    pub description: String,
}

#[derive(Debug, Serialize)]
pub struct SafeObject {
    pub name: String,
    pub kind: String,
    pub roleband: String,
    pub lanes: String,
}

#[derive(Debug, Serialize)]
pub struct WorkloadNodeWindow {
    pub nodeid: String,
    pub region: String,
    pub window_start_utc: String,
    pub window_end_utc: String,
    pub total_req_j: f64,
    pub total_surplus_j: f64,
    pub mean_vt_before: f64,
    pub mean_vt_after: f64,
    pub mean_rcarbon: Option<f64>,
    pub mean_rbiodiv: Option<f64>,
    pub accepts: u64,
    pub rejects: u64,
    pub reroutes: u64,
}

#[derive(Debug, Serialize)]
pub struct BlastRadiusSummary {
    pub nodeid: String,
    pub max_carbon_radius: f64,
    pub max_biodiv_radius: f64,
    pub vt_radius_sum: f64,
}

#[derive(Debug, Serialize)]
pub struct AiContextBundle {
    pub manifest: RepoManifest,
    pub safe_catalog: Vec<SafeObject>,
    pub workload_window: WorkloadNodeWindow,
    pub blast_radius: BlastRadiusSummary,
}

fn query_repo_manifest(conn: &Connection, reponame: &str) -> Result<RepoManifest, SpineError> {
    if reponame.is_empty() {
        return Err(SpineError::InvalidArgument(
            "Repository name cannot be empty".to_string(),
        ));
    }

    let mut stmt = conn.prepare(
        r#"
        SELECT reponame, roleband, lane, description
        FROM veconet_repo_manifest_agent
        WHERE reponame = ?1
        "#,
    )?;

    let row = stmt.query_row([reponame], |row| {
        Ok(RepoManifest {
            reponame: row.get(0)?,
            roleband: row.get(1)?,
            lane: row.get(2)?,
            description: row.get(3)?,
        })
    })?;

    Ok(row)
}

fn query_safe_catalog(conn: &Connection, reponame: &str) -> Result<Vec<SafeObject>, SpineError> {
    let mut stmt = conn.prepare(
        r#"
        SELECT objectname, kind, roleband, lanes
        FROM vagentsafecatalog
        WHERE reponame = ?1
        "#,
    )?;

    let rows = stmt.query_map([reponame], |row| {
        Ok(SafeObject {
            name: row.get(0)?,
            kind: row.get(1)?,
            roleband: row.get(2)?,
            lanes: row.get(3)?,
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

fn query_workload_node_window(
    conn: &Connection,
    nodeid: &str,
) -> Result<WorkloadNodeWindow, SpineError> {
    if nodeid.is_empty() {
        return Err(SpineError::InvalidArgument(
            "Node ID cannot be empty".to_string(),
        ));
    }

    let mut stmt = conn.prepare(
        r#"
        SELECT
            nodeid,
            region,
            windowstartutc,
            windowendutc,
            totalreqj,
            totalsurplusj,
            meanvtbefore,
            meanvtafter,
            meanrcarbon,
            meanrbiodiv,
            accepts,
            rejects,
            reroutes
        FROM vcyboworkloadnodewindow
        WHERE nodeid = ?1
        "#,
    )?;

    let row = stmt.query_row([nodeid], |row| {
        Ok(WorkloadNodeWindow {
            nodeid: row.get(0)?,
            region: row.get(1)?,
            window_start_utc: row.get(2)?,
            window_end_utc: row.get(3)?,
            total_req_j: row.get(4)?,
            total_surplus_j: row.get(5)?,
            mean_vt_before: row.get(6)?,
            mean_vt_after: row.get(7)?,
            mean_rcarbon: row.get(8).ok(),
            mean_rbiodiv: row.get(9).ok(),
            accepts: row.get(10)?,
            rejects: row.get(11)?,
            reroutes: row.get(12)?,
        })
    })?;

    Ok(row)
}

fn query_blast_radius_summary(
    conn: &Connection,
    nodeid: &str,
) -> Result<BlastRadiusSummary, SpineError> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            nodeid,
            maxcarbonradius,
            maxbiodivradius,
            vtradiussum
        FROM vshardblastradius
        WHERE nodeid = ?1
        "#,
    )?;

    let row = stmt.query_row([nodeid], |row| {
        Ok(BlastRadiusSummary {
            nodeid: row.get(0)?,
            max_carbon_radius: row.get(1)?,
            max_biodiv_radius: row.get(2)?,
            vt_radius_sum: row.get(3)?,
        })
    })?;

    Ok(row)
}

#[no_mangle]
pub extern "C" fn econet_open_index(dbpath: *const c_char) -> *mut ShardIndex {
    if dbpath.is_null() {
        return ptr::null_mut();
    }

    let dbpath_str = match cstr_to_str(dbpath) {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };

    match ShardIndex::new(dbpath_str) {
        Ok(handle) => Box::into_raw(Box::new(handle)),
        Err(_) => ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn econet_close_index(handle: *mut ShardIndex) {
    if handle.is_null() {
        return;
    }
    unsafe {
        let _ = Box::from_raw(handle);
    }
}

impl_ffi_query! {
    #[no_mangle]
    pub extern "C" fn econet_get_ai_context_bundle(
        handle: *mut ShardIndex,
        nodeid: *const c_char,
        reponame: *const c_char,
    ) -> *mut c_char
    {
        if handle.is_null() {
            return Err(SpineError::InvalidArgument(
                "Invalid null handle provided to econet_get_ai_context_bundle".to_string(),
            ));
        }

        let nodeid_str = cstr_to_str(nodeid)?;
        let reponame_str = cstr_to_str(reponame)?;

        if nodeid_str.is_empty() {
            return Err(SpineError::InvalidArgument(
                "Node ID cannot be empty".to_string(),
            ));
        }
        if reponame_str.is_empty() {
            return Err(SpineError::InvalidArgument(
                "Repository name cannot be empty".to_string(),
            ));
        }

        let shard_index = unsafe { &*handle };

        let manifest = query_repo_manifest(&shard_index.conn, reponame_str)?;
        let safe_catalog = query_safe_catalog(&shard_index.conn, reponame_str)?;
        let workload_window = query_workload_node_window(&shard_index.conn, nodeid_str)?;
        let blast_radius = query_blast_radius_summary(&shard_index.conn, nodeid_str)?;

        let bundle = AiContextBundle {
            manifest,
            safe_catalog,
            workload_window,
            blast_radius,
        };

        Ok(bundle)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CString;

    #[test]
    fn test_error_json_internal_with_special_chars() {
        let msg = "Error with \"quotes\", newlines\nand\ttabs, and backslashes\\here";
        let ptr = error_json_internal(msg);
        assert!(!ptr.is_null());

        // Convert pointer back to CString for inspection (without dropping it prematurely)
        let cstr = unsafe { CStr::from_ptr(ptr) };
        let json_str = cstr.to_str().unwrap();
        let result: serde_json::Value = serde_json::from_str(json_str).expect("JSON should be valid");
        
        assert!(result.get("error").is_some());
        assert_eq!(result["error"], msg);
        
        // Clean up the allocated memory
        unsafe { let _ = CString::from_raw(ptr); }
    }

    #[test]
    fn test_cstr_to_str_valid_utf8() {
        let input = CString::new("hello world").unwrap();
        let result = cstr_to_str(input.as_ptr());
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), "hello world");
    }

    #[test]
    fn test_cstr_to_str_null_pointer() {
        let result = cstr_to_str(std::ptr::null());
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), SpineError::Utf8));
    }
}
