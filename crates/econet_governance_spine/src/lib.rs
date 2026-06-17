// Filename: crates/econet_governance_spine/src/lib.rs
// Destination: crates/econet_governance_spine/src/lib.rs

#![forbid(unsafe_code)]

use std::collections::HashMap;
use std::path::PathBuf;

use thiserror::Error;

use rusqlite::{params, Connection, Row};

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
        let conn = Connection::open(db_path)?;
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

    pub fn get_plane_weights(
        &self,
        shard_id: &str,
    ) -> Result<Vec<PlaneWeight>, SpineError> {
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
    ExpectedSchema { tables }
}
