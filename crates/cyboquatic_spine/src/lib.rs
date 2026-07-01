// eco_restoration_shard/crates/cyboquatic_spine/src/lib.rs
// Rust edition 2024
//
// Non-actuating cdylib that opens the Cyboquatic eco spine SQLite DB read-only.
// Exposes JSON-returning C ABI functions for KER targets, blast-radius overlays,
// workload windows, biodegradable substrate summaries, and governance guards.

#![deny(unsafe_code)]

use std::path::Path;
use std::time::{SystemTime, UNIX_EPOCH};

use chrono::{DateTime, Utc};
use rusqlite::{params, Connection, OpenFlags, OptionalExtension, Row};
use serde::{Deserialize, Serialize};
use thiserror::Error;

fn utc_now_seconds() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs() as i64
}

#[derive(Debug, Error)]
pub enum SpineError {
    #[error("SQLite error: {0}")]
    Sqlite(#[from] rusqlite::Error),
    #[error("Time parse error: {0}")]
    TimeParse(#[from] chrono::ParseError),
    #[error("Schema mismatch: {0}")]
    SchemaMismatch(String),
    #[error("Missing definition registry entry for id={0}")]
    MissingDefinitionRegistry(String),
    #[error("Missing lane status for shard={0}")]
    MissingLaneStatus(String),
    #[error("Missing KER residual for shard={0}")]
    MissingKerResidual(String),
    #[error("Missing plane weights for shard={0}")]
    MissingPlaneWeights(String),
    #[error("Missing blast radius for shard={0}")]
    MissingBlastRadius(String),
    #[error("Missing eco wealth statement for steward={0}")]
    MissingEcoWealth(String),
    #[error("Missing Cyboquatic metrics for node={0}")]
    MissingCyboquaticMetrics(String),
    #[error("Lane not admissible: {0}")]
    LaneNotAdmissible(String),
    #[error("Cyboquatic guard failed: {0}")]
    CyboquaticGuardFailed(String),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MachineryBlastRadius {
    pub machine_id: String,
    pub kind: String,
    pub region: String,
    pub lane: String,
    pub max_node_radius: f64,
    pub max_region_radius: f64,
    pub max_energy_radius: f64,
    pub max_carbon_radius: f64,
    pub max_biodiv_radius: f64,
    pub vt_radius_sum: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NodeWindowSummary {
    pub node_id: String,
    pub region: String,
    pub window_start_utc: DateTime<Utc>,
    pub window_end_utc: DateTime<Utc>,
    pub total_e_req_j: f64,
    pub total_e_used_j: f64,
    pub total_carbon_kg: f64,
    pub total_pollutant_mass_kg: f64,
    pub mean_vt_before: f64,
    pub mean_vt_after: f64,
    pub mean_roh_scalar: f64,
    pub accepts: u64,
    pub derates: u64,
    pub rejects: u64,
    pub delta_vt: f64,
    pub pollutant_per_kj: f64,
}

#[derive(Debug, Serialize)]
pub struct KerTarget {
    pub node_id: String,
    pub k_metric: f64,
    pub e_metric: f64,
    pub r_metric: f64,
    pub vt_max: f64,
    pub ker_deployable: bool,
}

#[derive(Debug, Serialize)]
pub struct BlastRadiusEntry {
    pub source_type: String,
    pub source_id: String,
    pub target_type: String,
    pub target_id: String,
    pub impact_plane: String,
    pub impact_score_sum: f64,
    pub vt_sensitivity_mean: f64,
    pub link_count: i64,
}

#[derive(Debug, Serialize)]
pub struct WorkloadWindowEntry {
    pub node_id: String,
    pub channel: String,
    pub window_start_utc: String,
    pub window_end_utc: String,
    pub total_requests_j: f64,
    pub total_surplus_j: f64,
    pub accepted_requests_j: f64,
    pub rejected_requests_j: f64,
    pub rerouted_requests_j: f64,
    pub mean_vt_before: f64,
    pub mean_vt_after: f64,
    pub mean_delta_vt: f64,
    pub mean_r_carbon: Option<f64>,
    pub mean_r_biodiv: Option<f64>,
    pub mean_r_materials: Option<f64>,
    pub mean_r_water: Option<f64>,
    pub accept_fraction: Option<f64>,
}

#[derive(Debug, Serialize)]
pub struct SubstrateSummaryEntry {
    pub node_id: String,
    pub material_id: String,
    pub first_start_utc: String,
    pub last_end_utc: String,
    pub mean_k: f64,
    pub mean_e: f64,
    pub mean_r: f64,
    pub vt_min: f64,
    pub vt_max: f64,
    pub deployable_count: i64,
    pub window_count: i64,
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

#[derive(Debug, Clone)]
pub struct ExpectedTable {
    pub name: String,
    pub columns: Vec<ExpectedColumn>,
}

#[derive(Debug, Clone)]
pub struct ExpectedColumn {
    pub name: String,
    pub datatype: String,
    pub not_null: bool,
}

#[derive(Debug, Clone)]
pub struct ExpectedSchema {
    pub tables: Vec<ExpectedTable>,
}

pub struct SchemaVerifier<'a> {
    conn: &'a Connection,
    expected: ExpectedSchema,
}

impl<'a> SchemaVerifier<'a> {
    pub fn verify(&self) -> Result<(), String> {
        for table in &self.expected.tables {
            self.verify_table(table)?;
        }
        Ok(())
    }

    fn verify_table(&self, table: &ExpectedTable) -> Result<(), String> {
        let pragma_sql = format!("PRAGMA table_info({})", table.name);
        let mut stmt = self.conn.prepare(&pragma_sql).map_err(|e| e.to_string())?;
        let mut rows = stmt.query([]).map_err(|e| e.to_string())?;
        let mut actual_cols: Vec<ExpectedColumn> = Vec::new();
        
        while let Some(row) = rows.next().map_err(|e| e.to_string())? {
            actual_cols.push(map_table_info_row(&row)?);
        }

        for expected_col in &table.columns {
            let actual = actual_cols
                .iter()
                .find(|c| c.name == expected_col.name)
                .ok_or_else(|| {
                    format!(
                        "missing column '{}' on table '{}'",
                        expected_col.name, table.name
                    )
                })?;

            if actual.not_null != expected_col.not_null {
                return Err(format!(
                    "column '{}' on table '{}' has notnull={}, expected={}",
                    expected_col.name, table.name, actual.not_null, expected_col.not_null
                ));
            }
        }
        Ok(())
    }
}

fn map_table_info_row(row: &Row<'_>) -> Result<ExpectedColumn, String> {
    let name: String = row.get(1).map_err(|e| e.to_string())?;
    let datatype: String = row.get(2).map_err(|e| e.to_string())?;
    let notnull_flag: i64 = row.get(3).map_err(|e| e.to_string())?;
    Ok(ExpectedColumn {
        name,
        datatype,
        not_null: notnull_flag != 0,
    })
}

pub fn cyboquatic_expected_schema() -> ExpectedSchema {
    let tables = vec![
        ExpectedTable {
            name: "lanestatusshard".to_string(),
            columns: vec![
                ExpectedColumn { name: "shardid".to_string(), datatype: "TEXT".to_string(), not_null: true },
                ExpectedColumn { name: "region".to_string(), datatype: "TEXT".to_string(), not_null: true },
                ExpectedColumn { name: "lane".to_string(), datatype: "TEXT".to_string(), not_null: true },
                ExpectedColumn { name: "verdict".to_string(), datatype: "TEXT".to_string(), not_null: true },
                ExpectedColumn { name: "kerk".to_string(), datatype: "REAL".to_string(), not_null: true },
                ExpectedColumn { name: "kere".to_string(), datatype: "REAL".to_string(), not_null: true },
                ExpectedColumn { name: "kerr".to_string(), datatype: "REAL".to_string(), not_null: true },
                ExpectedColumn { name: "residualvt".to_string(), datatype: "REAL".to_string(), not_null: true },
                ExpectedColumn { name: "maxstalenesshours".to_string(), datatype: "INTEGER".to_string(), not_null: true },
                ExpectedColumn { name: "expiresutc".to_string(), datatype: "INTEGER".to_string(), not_null: true },
                ExpectedColumn { name: "carbonnegativeok".to_string(), datatype: "INTEGER".to_string(), not_null: true },
                ExpectedColumn { name: "restorationok".to_string(), datatype: "INTEGER".to_string(), not_null: true },
            ],
        },
        ExpectedTable {
            name: "definitionregistry".to_string(),
            columns: vec![
                ExpectedColumn { name: "defid".to_string(), datatype: "TEXT".to_string(), not_null: true },
                ExpectedColumn { name: "logicalname".to_string(), datatype: "TEXT".to_string(), not_null: true },
                ExpectedColumn { name: "repopath".to_string(), datatype: "TEXT".to_string(), not_null: true },
                ExpectedColumn { name: "category".to_string(), datatype: "TEXT".to_string(), not_null: true },
                ExpectedColumn { name: "frozen".to_string(), datatype: "INTEGER".to_string(), not_null: true },
            ],
        },
        ExpectedTable {
            name: "kerresidual".to_string(),
            columns: vec![
                ExpectedColumn { name: "shardid".to_string(), datatype: "TEXT".to_string(), not_null: true },
                ExpectedColumn { name: "region".to_string(), datatype: "TEXT".to_string(), not_null: true },
                ExpectedColumn { name: "kerk".to_string(), datatype: "REAL".to_string(), not_null: true },
                ExpectedColumn { name: "kere".to_string(), datatype: "REAL".to_string(), not_null: true },
                ExpectedColumn { name: "kerr".to_string(), datatype: "REAL".to_string(), not_null: true },
                ExpectedColumn { name: "residualvt".to_string(), datatype: "REAL".to_string(), not_null: true },
            ],
        },
    ];
    ExpectedSchema { tables }
}

pub struct CyboquaticSpine {
    conn: Connection,
    expected_schema: ExpectedSchema,
}

impl CyboquaticSpine {
    pub fn open<P: AsRef<Path>>(path: P, expected_schema: ExpectedSchema) -> Result<Self, SpineError> {
        let conn = Connection::open(path)?;
        conn.execute_batch("PRAGMA foreign_keys = ON;")?;
        let spine = Self { conn, expected_schema };
        spine.verify_schema()?;
        Ok(spine)
    }

    fn verify_schema(&self) -> Result<(), SpineError> {
        let verifier = SchemaVerifier {
            conn: &self.conn,
            expected: self.expected_schema.clone(),
        };
        verifier.verify().map_err(SpineError::SchemaMismatch)
    }

    pub fn list_machinery_blastradius(&self) -> Result<Vec<MachineryBlastRadius>, SpineError> {
        let mut stmt = self.conn.prepare(
            r#"SELECT machine_id, kind, region, lane,
                      max_node_radius, max_region_radius, max_energy_radius,
                      max_carbon_radius, max_biodiv_radius, vt_radius_sum
               FROM v_cybo_mach_blastradius_agg"#,
        )?;

        let iter = stmt.query_map([], |row| {
            Ok(MachineryBlastRadius {
                machine_id: row.get(0)?,
                kind: row.get(1)?,
                region: row.get(2)?,
                lane: row.get(3)?,
                max_node_radius: row.get(4)?,
                max_region_radius: row.get(5)?,
                max_energy_radius: row.get(6)?,
                max_carbon_radius: row.get(7)?,
                max_biodiv_radius: row.get(8)?,
                vt_radius_sum: row.get(9)?,
            })
        })?;

        let mut out = Vec::new();
        for r in iter {
            out.push(r?);
        }
        Ok(out)
    }

    pub fn summarize_node_region(
        &self,
        node_id: &str,
        region: &str,
    ) -> Result<Option<NodeWindowSummary>, SpineError> {
        let mut stmt = self.conn.prepare(
            r#"SELECT node_id, region, window_start_utc, window_end_utc,
                      total_e_req_j, total_e_used_j, total_carbon_kg, total_pollutant_mass_kg,
                      mean_vt_before, mean_vt_after, mean_roh_scalar,
                      accepts, derates, rejects
               FROM v_cybo_mach_node_window
               WHERE node_id = ?1 AND region = ?2"#,
        )?;

        let res = stmt.query_row(params![node_id, region], |row| {
            let start: String = row.get(2)?;
            let end: String = row.get(3)?;
            let start_dt = start.parse::<DateTime<Utc>>()?;
            let end_dt = end.parse::<DateTime<Utc>>()?;

            let total_e_used: f64 = row.get(5)?;
            let total_poll: f64 = row.get(7)?;
            let pollutant_per_kj = if total_e_used > 0.0 {
                total_poll / (total_e_used / 1000.0)
            } else {
                0.0
            };

            let mean_vt_before: f64 = row.get(8)?;
            let mean_vt_after: f64 = row.get(9)?;
            let delta_vt = mean_vt_after - mean_vt_before;

            Ok(NodeWindowSummary {
                node_id: row.get(0)?,
                region: row.get(1)?,
                window_start_utc: start_dt,
                window_end_utc: end_dt,
                total_e_req_j: row.get(4)?,
                total_e_used_j: total_e_used,
                total_carbon_kg: row.get(6)?,
                total_pollutant_mass_kg: total_poll,
                mean_vt_before,
                mean_vt_after,
                mean_roh_scalar: row.get(10)?,
                accepts: row.get::<_, i64>(11)? as u64,
                derates: row.get::<_, i64>(12)? as u64,
                rejects: row.get::<_, i64>(13)? as u64,
                delta_vt,
                pollutant_per_kj,
            })
        });

        match res {
            Ok(summary) => Ok(Some(summary)),
            Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
            Err(e) => Err(SpineError::Sqlite(e)),
        }
    }

    pub fn get_ker_residual(&self, shard_id: &str) -> Result<KerResidual, SpineError> {
        let mut stmt = self.conn.prepare(
            "SELECT shardid, region, kerk, kere, kerr, residualvt \
             FROM vresidualkernel WHERE shardid = ?1",
        )?;
        let opt: Option<KerResidual> = stmt
            .query_row(params![shard_id], map_ker_residual)
            .optional()?;
        opt.ok_or_else(|| SpineError::MissingKerResidual(shard_id.to_string()))
    }

    pub fn get_plane_weights(&self, shard_id: &str) -> Result<Vec<PlaneWeight>, SpineError> {
        let mut stmt = self.conn.prepare(
            "SELECT planename, weight, nonoffsettable \
             FROM vplaneweights WHERE shardid = ?1",
        )?;
        let mut rows = stmt.query(params![shard_id])?;
        let mut result = Vec::new();
        while let Some(row) = rows.next()? {
            result.push(map_plane_weight(row)?);
        }
        if result.is_empty() {
            Err(SpineError::MissingPlaneWeights(shard_id.to_string()))
        } else {
            Ok(result)
        }
    }

    pub fn get_blast_radius(&self, shard_id: &str) -> Result<BlastRadius, SpineError> {
        let mut stmt = self.conn.prepare(
            "SELECT shardid, radiusmeters, adjacencycount \
             FROM vshardblast WHERE shardid = ?1",
        )?;
        let opt: Option<BlastRadius> = stmt
            .query_row(params![shard_id], map_blast_radius)
            .optional()?;
        opt.ok_or_else(|| SpineError::MissingBlastRadius(shard_id.to_string()))
    }

    pub fn get_lane_status(&self, shard_id: &str) -> Result<LaneStatus, SpineError> {
        let mut stmt = self.conn.prepare(
            "SELECT shardid, region, lane, verdict, kerk, kere, kerr, residualvt, \
                    maxstalenesshours, expiresutc, carbonnegativeok, restorationok \
             FROM vlaneadmissibility WHERE shardid = ?1",
        )?;
        let opt: Option<LaneStatus> = stmt
            .query_row(params![shard_id], map_lane_status)
            .optional()?;
        opt.ok_or_else(|| SpineError::MissingLaneStatus(shard_id.to_string()))
    }

    pub fn get_definition_registry_entry(
        &self,
        def_id: &str,
    ) -> Result<DefinitionRegistryEntry, SpineError> {
        let mut stmt = self.conn.prepare(
            "SELECT defid, logicalname, repopath, category, frozen \
             FROM definitionregistry WHERE defid = ?1",
        )?;
        let opt: Option<DefinitionRegistryEntry> = stmt
            .query_row(params![def_id], map_definition_registry)
            .optional()?;
        opt.ok_or_else(|| SpineError::MissingDefinitionRegistry(def_id.to_string()))
    }

    pub fn get_eco_wealth_latest(
        &self,
        steward_did: &str,
    ) -> Result<EcoWealthStatement, SpineError> {
        let mut stmt = self.conn.prepare(
            "SELECT stewarddid, shardid, wealthscore, keffective, eeffective, reffective \
             FROM vstewardecowealthlatest WHERE stewarddid = ?1",
        )?;
        let opt: Option<EcoWealthStatement> = stmt
            .query_row(params![steward_did], map_eco_wealth)
            .optional()?;
        opt.ok_or_else(|| SpineError::MissingEcoWealth(steward_did.to_string()))
    }

    pub fn get_cyboquatic_metrics(
        &self,
        node_id: &str,
    ) -> Result<CyboquaticMetrics, SpineError> {
        let mut stmt = self.conn.prepare(
            "SELECT nodeid, shardid, ecoperjoule, carbonnegativeok, restorationok \
             FROM vcyboquaticecoperjoule WHERE nodeid = ?1",
        )?;
        let opt: Option<CyboquaticMetrics> = stmt
            .query_row(params![node_id], map_cyboquatic)
            .optional()?;
        opt.ok_or_else(|| SpineError::MissingCyboquaticMetrics(node_id.to_string()))
    }

    pub fn check_cyboquatic_node_allowed(
        &self,
        node_id: &str,
        shard_id: &str,
    ) -> Result<(), SpineError> {
        let lane = self.get_lane_status(shard_id)?;
        let now = utc_now_seconds();
        let lane_guard_result = lane_guard_check(&lane, LaneFilter::ExactProdOrExpProd, now);
        
        if !lane_guard_result.admissible {
            return Err(SpineError::LaneNotAdmissible(
                lane_guard_result.reason.unwrap_or_else(|| "lane not admissible".to_string()),
            ));
        }

        let ker = self.get_ker_residual(shard_id)?;
        let blast = self.get_blast_radius(shard_id)?;
        let plane_weights = self.get_plane_weights(shard_id)?;
        let cybo = self.get_cyboquatic_metrics(node_id)?;

        let mt_inputs = Mt6883GuardInputs {
            ker,
            lane: lane.clone(),
            blast,
            plane_weights,
        };
        let mt_result = Mt6883Guard::check(&mt_inputs);
        if !mt_result.allowed {
            return Err(SpineError::CyboquaticGuardFailed(
                mt_result.reason.unwrap_or_else(|| "mt6883 guard failed".to_string()),
            ));
        }

        let cybo_guard_result = cyboquatic_guard_check(&cybo);
        if !cybo_guard_result.allowed {
            return Err(SpineError::CyboquaticGuardFailed(
                cybo_guard_result.reason.unwrap_or_else(|| "cyboquatic guard failed".to_string()),
            ));
        }

        Ok(())
    }
}

fn map_ker_residual(row: &Row<'_>) -> Result<KerResidual, rusqlite::Error> {
    Ok(KerResidual {
        shard_id: row.get(0)?,
        region: row.get(1)?,
        k: row.get(2)?,
        e: row.get(3)?,
        r: row.get(4)?,
        vt: row.get(5)?,
    })
}

fn map_plane_weight(row: &Row<'_>) -> Result<PlaneWeight, rusqlite::Error> {
    let non_offsettable_int: i64 = row.get(2)?;
    Ok(PlaneWeight {
        plane_name: row.get(0)?,
        weight: row.get(1)?,
        non_offsettable: non_offsettable_int != 0,
    })
}

fn map_blast_radius(row: &Row<'_>) -> Result<BlastRadius, rusqlite::Error> {
    Ok(BlastRadius {
        shard_id: row.get(0)?,
        radius_meters: row.get(1)?,
        adjacency_count: row.get(2)?,
    })
}

fn map_lane_status(row: &Row<'_>) -> Result<LaneStatus, rusqlite::Error> {
    let carbon_negative_int: i64 = row.get(10)?;
    let restoration_int: i64 = row.get(11)?;
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
        carbon_negative_ok: carbon_negative_int != 0,
        restoration_ok: restoration_int != 0,
    })
}

fn map_definition_registry(row: &Row<'_>) -> Result<DefinitionRegistryEntry, rusqlite::Error> {
    let frozen_int: i64 = row.get(4)?;
    Ok(DefinitionRegistryEntry {
        def_id: row.get(0)?,
        logical_name: row.get(1)?,
        repo_path: row.get(2)?,
        category: row.get(3)?,
        frozen: frozen_int != 0,
    })
}

fn map_eco_wealth(row: &Row<'_>) -> Result<EcoWealthStatement, rusqlite::Error> {
    Ok(EcoWealthStatement {
        steward_did: row.get(0)?,
        shard_id: row.get(1)?,
        wealth_score: row.get(2)?,
        k_effective: row.get(3)?,
        e_effective: row.get(4)?,
        r_effective: row.get(5)?,
    })
}

fn map_cyboquatic(row: &Row<'_>) -> Result<CyboquaticMetrics, rusqlite::Error> {
    let carbon_negative_int: i64 = row.get(3)?;
    let restoration_int: i64 = row.get(4)?;
    Ok(CyboquaticMetrics {
        node_id: row.get(0)?,
        shard_id: row.get(1)?,
        eco_per_joule: row.get(2)?,
        carbon_negative_ok: carbon_negative_int != 0,
        restoration_ok: restoration_int != 0,
    })
}

#[derive(Debug, Clone, Copy)]
pub enum LaneFilter {
    ExactProd,
    ExactExpProd,
    ExactProdOrExpProd,
}

#[derive(Debug, Clone)]
pub struct KerGuardInputs {
    pub old_k: f64,
    pub old_e: f64,
    pub old_r: f64,
    pub new_k: f64,
    pub new_e: f64,
    pub new_r: f64,
}

#[derive(Debug, Clone)]
pub struct KerGuardResult {
    pub ok: bool,
    pub reason: Option<String>,
}

pub struct KerUpgradeGuard;

impl KerUpgradeGuard {
    pub fn check(inputs: &KerGuardInputs) -> KerGuardResult {
        if inputs.new_k < inputs.old_k {
            return KerGuardResult { ok: false, reason: Some(format!("new K {:.6} < old K {:.6}", inputs.new_k, inputs.old_k)) };
        }
        if inputs.new_e < inputs.old_e {
            return KerGuardResult { ok: false, reason: Some(format!("new E {:.6} < old E {:.6}", inputs.new_e, inputs.old_e)) };
        }
        if inputs.new_r > inputs.old_r {
            return KerGuardResult { ok: false, reason: Some(format!("new R {:.6} > old R {:.6}", inputs.new_r, inputs.old_r)) };
        }
        KerGuardResult { ok: true, reason: None }
    }
}

#[derive(Debug, Clone)]
pub struct LaneGuardInputs {
    pub lane_status: LaneStatus,
    pub filter: LaneFilter,
    pub now_utc: i64,
}

#[derive(Debug, Clone)]
pub struct LaneGuardResult {
    pub admissible: bool,
    pub reason: Option<String>,
}

fn lane_guard_check(lane_status: &LaneStatus, filter: LaneFilter, now_utc: i64) -> LaneGuardResult {
    if now_utc > lane_status.expires_utc {
        return LaneGuardResult { admissible: false, reason: Some("lane verdict is stale".to_string()) };
    }

    match filter {
        LaneFilter::ExactProd => {
            if lane_status.lane != "PROD" {
                return LaneGuardResult { admissible: false, reason: Some(format!("lane is not PROD: {}", lane_status.lane)) };
            }
        }
        LaneFilter::ExactExpProd => {
            if lane_status.lane != "EXPPROD" {
                return LaneGuardResult { admissible: false, reason: Some(format!("lane is not EXPPROD: {}", lane_status.lane)) };
            }
        }
        LaneFilter::ExactProdOrExpProd => {
            if lane_status.lane != "PROD" && lane_status.lane != "EXPPROD" {
                return LaneGuardResult { admissible: false, reason: Some(format!("lane is not PROD or EXPPROD: {}", lane_status.lane)) };
            }
        }
    }

    if !lane_status.carbon_negative_ok {
        return LaneGuardResult { admissible: false, reason: Some("carbonnegativeok is false".to_string()) };
    }
    if !lane_status.restoration_ok {
        return LaneGuardResult { admissible: false, reason: Some("restorationok is false".to_string()) };
    }

    LaneGuardResult { admissible: true, reason: None }
}

#[derive(Debug, Clone)]
pub struct Mt6883GuardInputs {
    pub ker: KerResidual,
    pub lane: LaneStatus,
    pub blast: BlastRadius,
    pub plane_weights: Vec<PlaneWeight>,
}

#[derive(Debug, Clone)]
pub struct Mt6883GuardResult {
    pub allowed: bool,
    pub reason: Option<String>,
}

pub struct Mt6883Guard;

impl Mt6883Guard {
    pub fn check(inputs: &Mt6883GuardInputs) -> Mt6883GuardResult {
        if inputs.ker.k < 0.90 {
            return Mt6883GuardResult { allowed: false, reason: Some("K below 0.90".to_string()) };
        }
        if inputs.ker.e < 0.90 {
            return Mt6883GuardResult { allowed: false, reason: Some("E below 0.90".to_string()) };
        }
        if inputs.ker.r > 0.13 {
            return Mt6883GuardResult { allowed: false, reason: Some("R above 0.13".to_string()) };
        }

        if inputs.blast.radius_meters > 0.0 && inputs.blast.adjacency_count > 0 {
            if inputs.ker.r > 0.10 {
                return Mt6883GuardResult { allowed: false, reason: Some("blast radius too large for residual R".to_string()) };
            }
        }

        let non_offsettable_violation = inputs.plane_weights.iter().any(|p| p.non_offsettable && p.weight <= 0.0);
        if non_offsettable_violation {
            return Mt6883GuardResult { allowed: false, reason: Some("non-offsettable plane weight invalid".to_string()) };
        }

        Mt6883GuardResult { allowed: true, reason: None }
    }
}

#[derive(Debug, Clone)]
pub struct CyboquaticGuardResult {
    pub allowed: bool,
    pub reason: Option<String>,
}

fn cyboquatic_guard_check(metrics: &CyboquaticMetrics) -> CyboquaticGuardResult {
    if !metrics.carbon_negative_ok {
        return CyboquaticGuardResult { allowed: false, reason: Some("carbonnegativeok flag is false".to_string()) };
    }
    if !metrics.restoration_ok {
        return CyboquaticGuardResult { allowed: false, reason: Some("restorationok flag is false".to_string()) };
    }
    if metrics.eco_per_joule <= 0.0 {
        return CyboquaticGuardResult { allowed: false, reason: Some("ecoperjoule must be positive".to_string()) };
    }
    CyboquaticGuardResult { allowed: true, reason: None }
}

fn query_ker_for_node(conn: &Connection, node_id: &str) -> rusqlite::Result<KerTarget> {
    let mut stmt = conn.prepare(
        r#"SELECT node_id, AVG(k_metric), AVG(e_metric), AVG(r_metric), MAX(vt_max), MAX(ker_deployable)
           FROM cybo_substrate_window WHERE node_id = ?1 GROUP BY node_id LIMIT 1"#,
    )?;
    stmt.query_row([node_id], |row| {
        let ker_deployable_i: i64 = row.get(5)?;
        Ok(KerTarget {
            node_id: row.get(0)?,
            k_metric: row.get(1)?,
            e_metric: row.get(2)?,
            r_metric: row.get(3)?,
            vt_max: row.get(4)?,
            ker_deployable: ker_deployable_i != 0,
        })
    })
}

fn query_blastradius_for_node(conn: &Connection, node_id: &str) -> rusqlite::Result<Vec<BlastRadiusEntry>> {
    let mut stmt = conn.prepare(
        r#"SELECT source_type, source_id, target_type, target_id, impact_plane, 
                  impact_score_sum, vt_sensitivity_mean, link_count
           FROM v_cybo_node_blastradius
           WHERE target_type = 'NODE' AND target_id = ?1
           ORDER BY impact_plane, source_type, source_id"#,
    )?;
    let rows = stmt.query_map([node_id], |row| {
        Ok(BlastRadiusEntry {
            source_type: row.get(0)?,
            source_id: row.get(1)?,
            target_type: row.get(2)?,
            target_id: row.get(3)?,
            impact_plane: row.get(4)?,
            impact_score_sum: row.get(5)?,
            vt_sensitivity_mean: row.get(6)?,
            link_count: row.get(7)?,
        })
    })?;
    let mut out = Vec::new();
    for r in rows { out.push(r?); }
    Ok(out)
}

fn query_workload_windows_for_node(conn: &Connection, node_id: &str) -> rusqlite::Result<Vec<WorkloadWindowEntry>> {
    let mut stmt = conn.prepare(
        r#"SELECT node_id, channel, window_start_utc, window_end_utc, total_requests_j, total_surplus_j,
                  accepted_requests_j, rejected_requests_j, rerouted_requests_j, mean_vt_before, mean_vt_after,
                  mean_delta_vt, mean_r_carbon, mean_r_biodiv, mean_r_materials, mean_r_water, accept_fraction
           FROM v_cybo_workload_window WHERE node_id = ?1 ORDER BY channel, window_start_utc"#,
    )?;
    let rows = stmt.query_map([node_id], |row| {
        Ok(WorkloadWindowEntry {
            node_id: row.get(0)?, channel: row.get(1)?, window_start_utc: row.get(2)?, window_end_utc: row.get(3)?,
            total_requests_j: row.get(4)?, total_surplus_j: row.get(5)?, accepted_requests_j: row.get(6)?,
            rejected_requests_j: row.get(7)?, rerouted_requests_j: row.get(8)?, mean_vt_before: row.get(9)?,
            mean_vt_after: row.get(10)?, mean_delta_vt: row.get(11)?, mean_r_carbon: row.get(12).ok(),
            mean_r_biodiv: row.get(13).ok(), mean_r_materials: row.get(14).ok(), mean_r_water: row.get(15).ok(),
            accept_fraction: row.get(16).ok(),
        })
    })?;
    let mut out = Vec::new();
    for r in rows { out.push(r?); }
    Ok(out)
}

fn query_substrate_summary_for_node(conn: &Connection, node_id: &str) -> rusqlite::Result<Vec<SubstrateSummaryEntry>> {
    let mut stmt = conn.prepare(
        r#"SELECT node_id, material_id, first_start_utc, last_end_utc, mean_k, mean_e, mean_r, 
                  vt_min, vt_max, deployable_count, window_count
           FROM v_cybo_substrate_summary WHERE node_id = ?1 ORDER BY material_id"#,
    )?;
    let rows = stmt.query_map([node_id], |row| {
        Ok(SubstrateSummaryEntry {
            node_id: row.get(0)?, material_id: row.get(1)?, first_start_utc: row.get(2)?, last_end_utc: row.get(3)?,
            mean_k: row.get(4)?, mean_e: row.get(5)?, mean_r: row.get(6)?, vt_min: row.get(7)?, vt_max: row.get(8)?,
            deployable_count: row.get(9)?, window_count: row.get(10)?,
        })
    })?;
    let mut out = Vec::new();
    for r in rows { out.push(r?); }
    Ok(out)
}

mod ffi {
    #![allow(unsafe_code)]
    use super::*;
    use std::ffi::{CStr, CString};
    use std::os::raw::c_char;
    use std::path::Path;
    use rusqlite::{Connection, OpenFlags};

    fn cstr_to_str(ptr: *const c_char) -> Result<&'static str, &'static str> {
        if ptr.is_null() { return Err("null pointer"); }
        unsafe { CStr::from_ptr(ptr) }.to_str().map_err(|_| "invalid UTF-8")
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
        #[derive(Serialize)]
        struct ErrWrap<'a> { error: &'a str }
        to_json_c_string(&ErrWrap { error: msg })
    }

    fn open_ro_db(db_path: &str) -> rusqlite::Result<Connection> {
        Connection::open_with_flags(
            Path::new(db_path),
            OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_NO_MUTEX,
        )
    }

    #[no_mangle]
    pub extern "C" fn cybo_list_machinery_blastradius_json(db_path: *const c_char) -> *mut c_char {
        let db = match cstr_to_str(db_path) { Ok(s) => s, Err(m) => return error_json(m) };
        let spine = match CyboquaticSpine::open(db, cyboquatic_expected_schema()) {
            Ok(s) => s,
            Err(e) => return error_json(&format!("spine error: {}", e)),
        };
        match spine.list_machinery_blastradius() {
            Ok(data) => to_json_c_string(&data),
            Err(_) => error_json("machinery blast-radius query failed"),
        }
    }

    #[no_mangle]
    pub extern "C" fn cybo_summarize_node_region_json(
        db_path: *const c_char, node_id: *const c_char, region: *const c_char,
    ) -> *mut c_char {
        let db = match cstr_to_str(db_path) { Ok(s) => s, Err(m) => return error_json(m) };
        let node = match cstr_to_str(node_id) { Ok(s) => s, Err(m) => return error_json(m) };
        let regn = match cstr_to_str(region) { Ok(s) => s, Err(m) => return error_json(m) };
        let spine = match CyboquaticSpine::open(db, cyboquatic_expected_schema()) {
            Ok(s) => s,
            Err(e) => return error_json(&format!("spine error: {}", e)),
        };
        match spine.summarize_node_region(node, regn) {
            Ok(summary) => to_json_c_string(&summary),
            Err(_) => error_json("node region summary query failed"),
        }
    }

    #[no_mangle]
    pub extern "C" fn cybo_get_ker_for_node(db_path: *const c_char, node_id: *const c_char) -> *mut c_char {
        let db = match cstr_to_str(db_path) { Ok(s) => s, Err(m) => return error_json(m) };
        let node = match cstr_to_str(node_id) { Ok(s) => s, Err(m) => return error_json(m) };
        let conn = match open_ro_db(db) { Ok(c) => c, Err(_) => return error_json("failed to open SQLite spine") };
        match query_ker_for_node(&conn, node) {
            Ok(row) => to_json_c_string(&row),
            Err(_) => error_json("node not found or no substrate windows"),
        }
    }

    #[no_mangle]
    pub extern "C" fn cybo_get_blastradius_for_node(db_path: *const c_char, node_id: *const c_char) -> *mut c_char {
        let db = match cstr_to_str(db_path) { Ok(s) => s, Err(m) => return error_json(m) };
        let node = match cstr_to_str(node_id) { Ok(s) => s, Err(m) => return error_json(m) };
        let conn = match open_ro_db(db) { Ok(c) => c, Err(_) => return error_json("failed to open SQLite spine") };
        match query_blastradius_for_node(&conn, node) {
            Ok(rows) => to_json_c_string(&rows),
            Err(_) => error_json("blast-radius query failed"),
        }
    }

    #[no_mangle]
    pub extern "C" fn cybo_get_workload_windows_for_node(db_path: *const c_char, node_id: *const c_char) -> *mut c_char {
        let db = match cstr_to_str(db_path) { Ok(s) => s, Err(m) => return error_json(m) };
        let node = match cstr_to_str(node_id) { Ok(s) => s, Err(m) => return error_json(m) };
        let conn = match open_ro_db(db) { Ok(c) => c, Err(_) => return error_json("failed to open SQLite spine") };
        match query_workload_windows_for_node(&conn, node) {
            Ok(rows) => to_json_c_string(&rows),
            Err(_) => error_json("workload window query failed"),
        }
    }

    #[no_mangle]
    pub extern "C" fn cybo_get_substrate_summary_for_node(db_path: *const c_char, node_id: *const c_char) -> *mut c_char {
        let db = match cstr_to_str(db_path) { Ok(s) => s, Err(m) => return error_json(m) };
        let node = match cstr_to_str(node_id) { Ok(s) => s, Err(m) => return error_json(m) };
        let conn = match open_ro_db(db) { Ok(c) => c, Err(_) => return error_json("failed to open SQLite spine") };
        match query_substrate_summary_for_node(&conn, node) {
            Ok(rows) => to_json_c_string(&rows),
            Err(_) => error_json("substrate summary query failed"),
        }
    }

    #[no_mangle]
    pub extern "C" fn cybo_check_node_allowed(
        db_path: *const c_char, node_id: *const c_char, shard_id: *const c_char,
    ) -> *mut c_char {
        #[derive(Serialize)]
        struct GuardResultPayload { allowed: bool, reason: Option<String> }

        let db = match cstr_to_str(db_path) { Ok(s) => s, Err(m) => return error_json(m) };
        let node = match cstr_to_str(node_id) { Ok(s) => s, Err(m) => return error_json(m) };
        let shard = match cstr_to_str(shard_id) { Ok(s) => s, Err(m) => return error_json(m) };

        let spine = match CyboquaticSpine::open(db, cyboquatic_expected_schema()) {
            Ok(s) => s,
            Err(e) => return to_json_c_string(&GuardResultPayload { allowed: false, reason: Some(format!("schema or spine error: {}", e)) }),
        };

        let result = match spine.check_cyboquatic_node_allowed(node, shard) {
            Ok(()) => GuardResultPayload { allowed: true, reason: None },
            Err(e) => GuardResultPayload { allowed: false, reason: Some(e.to_string()) },
        };
        to_json_c_string(&result)
    }

    #[no_mangle]
    pub extern "C" fn cybo_free_json(ptr: *mut c_char) {
        if ptr.is_null() { return; }
        unsafe { let _ = CString::from_raw(ptr); }
    }
}
