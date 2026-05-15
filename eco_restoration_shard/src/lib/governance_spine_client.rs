// filename: governance_spine_client.rs
// destination: eco_restoration_shard/src/lib/governance_spine_client.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard
// logicalname: restoration.governance.spine.client.rust
// author: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7

//! # Governance Spine Client for eco_restoration_shard
//!
//! A Rust crate that wraps `restorationindex.sqlite3` via rusqlite,
//! exposing typed accessors for KER, blastradius, ecoperjoule, MT6883,
//! and identity bindings for cross-repo CI and agents.
//!
//! ## Example
//!
//! ```rust
//! use governance_spine_client::GovernanceSpineClient;
//!
//! let client = GovernanceSpineClient::open("db/restorationindex.sqlite3")?;
//! let nodes = client.get_restoration_nodes_phoenix()?;
//! for node in nodes {
//!     println!("Node: {}, Status: {}", node.node_id, node.restoration_status);
//! }
//! ```

use rusqlite::{Connection, Result as SqliteResult, params};
use std::path::Path;
use serde::{Deserialize, Serialize};

/// Author identifier for all governance artifacts
pub const AUTHOR_BOSTROM: &str = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";

/// Default database path
pub const DEFAULT_DB_PATH: &str = "db/restorationindex.sqlite3";

// =============================================================================
// Data Structures
// =============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RestorationNode {
    pub node_id: String,
    pub lane: String,
    pub domain: String,
    pub assetclass: String,
    pub blast_radius_km: f64,
    pub gw_risk_max: f64,
    pub restorationradius_m: f64,
    pub restoration_ok: bool,
    pub restoration_status: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoperjoulePolicy {
    pub policy_id: i32,
    pub lane: String,
    pub domain: String,
    pub assetclass: String,
    pub min_ecoperjoule: f64,
    pub max_ecoperjoule: f64,
    pub threshold_warning: f64,
    pub threshold_critical: f64,
    pub prod_eligible: bool,
    pub efficiency_status: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Mt6883Lane {
    pub lane_id: i32,
    pub lane_name: String,
    pub domain: String,
    pub status: String,
    pub mt6883_ok: bool,
    pub neuroethic_ok: bool,
    pub neuroethic_radius_hours: f64,
    pub capacity_pct: f64,
    pub latency_ms: f64,
    pub error_rate: f64,
    pub continuity_status: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IdentityBinding {
    pub binding_id: i32,
    pub bostrom_address: String,
    pub logicalname: String,
    pub file_path: String,
    pub region: String,
    pub scope: String,
    pub dbrole: String,
    pub role_description: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DefinitionRegistryEntry {
    pub definition_id: i32,
    pub logicalname: String,
    pub versiontag: String,
    pub artifact_type: String,
    pub file_path: String,
    pub status: String,
    pub region: String,
    pub scope: String,
    pub description: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GovernanceSummary {
    pub metric_category: String,
    pub total_count: i32,
    pub approved_count: i32,
    pub avg_score: f64,
}

// =============================================================================
// Governance Spine Client
// =============================================================================

pub struct GovernanceSpineClient {
    conn: Connection,
}

impl GovernanceSpineClient {
    /// Open a read-only connection to the governance database
    pub fn open<P: AsRef<Path>>(db_path: P) -> SqliteResult<Self> {
        let conn = Connection::open_with_flags(
            db_path,
            rusqlite::OpenFlags::SQLITE_OPEN_READ_ONLY,
        )?;
        Ok(Self { conn })
    }

    /// Open a connection (read-write mode for migrations)
    pub fn open_rw<P: AsRef<Path>>(db_path: P) -> SqliteResult<Self> {
        let conn = Connection::open(db_path)?;
        Ok(Self { conn })
    }

    // -------------------------------------------------------------------------
    // Restoration Nodes Queries
    // -------------------------------------------------------------------------

    /// Get all restoration nodes for Phoenix-AZ
    pub fn get_restoration_nodes_phoenix(&self) -> SqliteResult<Vec<RestorationNode>> {
        let mut stmt = self.conn.prepare(
            "SELECT 
                node_id, lane, domain, assetclass,
                blast_radius_km, gw_risk_max, restorationradius_m,
                restoration_ok,
                CASE 
                    WHEN restoration_ok = 1 AND gw_risk_max < 0.5 THEN 'APPROVED'
                    WHEN restoration_ok = 1 THEN 'CAUTION'
                    ELSE 'BLOCKED'
                END AS restoration_status
             FROM blastradiusindex
             WHERE region = 'PHOENIX-AZ'
             ORDER BY node_id"
        )?;

        let rows = stmt.query_map([], |row| {
            Ok(RestorationNode {
                node_id: row.get(0)?,
                lane: row.get(1)?,
                domain: row.get(2)?,
                assetclass: row.get(3)?,
                blast_radius_km: row.get(4)?,
                gw_risk_max: row.get(5)?,
                restorationradius_m: row.get(6)?,
                restoration_ok: row.get::<_, i32>(7)? == 1,
                restoration_status: row.get(8)?,
            })
        })?;

        rows.collect()
    }

    /// Get restoration-ready nodes (approved with low GW risk)
    pub fn get_restoration_ready_nodes(&self) -> SqliteResult<Vec<RestorationNode>> {
        let mut stmt = self.conn.prepare(
            "SELECT 
                node_id, lane, domain, assetclass,
                blast_radius_km, gw_risk_max, restorationradius_m,
                restoration_ok, 'READY' AS restoration_status
             FROM blastradiusindex
             WHERE region = 'PHOENIX-AZ' 
               AND restoration_ok = 1 
               AND gw_risk_max < 0.5
             ORDER BY restorationradius_m DESC"
        )?;

        let rows = stmt.query_map([], |row| {
            Ok(RestorationNode {
                node_id: row.get(0)?,
                lane: row.get(1)?,
                domain: row.get(2)?,
                assetclass: row.get(3)?,
                blast_radius_km: row.get(4)?,
                gw_risk_max: row.get(5)?,
                restorationradius_m: row.get(6)?,
                restoration_ok: row.get::<_, i32>(7)? == 1,
                restoration_status: row.get(8)?,
            })
        })?;

        rows.collect()
    }

    // -------------------------------------------------------------------------
    // Eco-Per-Joule Policy Queries
    // -------------------------------------------------------------------------

    /// Get all ecoperjoule policies for Phoenix production
    pub fn get_ecoperjoule_prod_phoenix(&self) -> SqliteResult<Vec<EcoperjoulePolicy>> {
        let mut stmt = self.conn.prepare(
            "SELECT 
                policy_id, lane, domain, assetclass,
                min_ecoperjoule, max_ecoperjoule,
                threshold_warning, threshold_critical,
                prod_eligible,
                CASE 
                    WHEN min_ecoperjoule >= max_ecoperjoule THEN 'OPTIMAL'
                    WHEN min_ecoperjoule >= threshold_warning THEN 'ACCEPTABLE'
                    WHEN min_ecoperjoule >= threshold_critical THEN 'MARGINAL'
                    ELSE 'CRITICAL'
                END AS efficiency_status
             FROM ecoperjoule_policy
             WHERE region = 'PHOENIX-AZ' AND prod_eligible = 1
             ORDER BY min_ecoperjoule DESC"
        )?;

        let rows = stmt.query_map([], |row| {
            Ok(EcoperjoulePolicy {
                policy_id: row.get(0)?,
                lane: row.get(1)?,
                domain: row.get(2)?,
                assetclass: row.get(3)?,
                min_ecoperjoule: row.get(4)?,
                max_ecoperjoule: row.get(5)?,
                threshold_warning: row.get(6)?,
                threshold_critical: row.get(7)?,
                prod_eligible: row.get::<_, i32>(8)? == 1,
                efficiency_status: row.get(9)?,
            })
        })?;

        rows.collect()
    }

    // -------------------------------------------------------------------------
    // MT6883 Lane Continuity Queries
    // -------------------------------------------------------------------------

    /// Get all MT6883 lanes for Phoenix-AZ
    pub fn get_mt6883_lanes_phoenix(&self) -> SqliteResult<Vec<Mt6883Lane>> {
        let mut stmt = self.conn.prepare(
            "SELECT 
                lane_id, lane_name, domain, status,
                mt6883_ok, neuroethic_ok, neuroethic_radius_hours,
                capacity_pct, latency_ms, error_rate,
                CASE 
                    WHEN mt6883_ok = 1 AND neuroethic_ok = 1 AND status = 'ACTIVE' THEN 'FULL_CONTINUITY'
                    WHEN mt6883_ok = 1 AND neuroethic_ok = 1 THEN 'DEGRADED_CONTINUITY'
                    ELSE 'CONTINUITY_FAILED'
                END AS continuity_status
             FROM lanestatusshard
             WHERE region = 'PHOENIX-AZ'
             ORDER BY lane_name"
        )?;

        let rows = stmt.query_map([], |row| {
            Ok(Mt6883Lane {
                lane_id: row.get(0)?,
                lane_name: row.get(1)?,
                domain: row.get(2)?,
                status: row.get(3)?,
                mt6883_ok: row.get::<_, i32>(4)? == 1,
                neuroethic_ok: row.get::<_, i32>(5)? == 1,
                neuroethic_radius_hours: row.get(6)?,
                capacity_pct: row.get(7)?,
                latency_ms: row.get(8)?,
                error_rate: row.get(9)?,
                continuity_status: row.get(10)?,
            })
        })?;

        rows.collect()
    }

    /// Get lanes requiring MT6883 attention
    pub fn get_mt6883_attention_required(&self) -> SqliteResult<Vec<Mt6883Lane>> {
        let mut stmt = self.conn.prepare(
            "SELECT 
                lane_id, lane_name, domain, status,
                mt6883_ok, neuroethic_ok, neuroethic_radius_hours,
                capacity_pct, latency_ms, error_rate,
                'ATTENTION_REQUIRED' AS continuity_status
             FROM lanestatusshard
             WHERE region = 'PHOENIX-AZ'
               AND (mt6883_ok = 0 OR neuroethic_ok = 0 OR neuroethic_radius_hours > 72)
             ORDER BY 
                CASE 
                    WHEN mt6883_ok = 0 AND neuroethic_ok = 0 THEN 1
                    WHEN mt6883_ok = 0 THEN 2
                    ELSE 3
                END"
        )?;

        let rows = stmt.query_map([], |row| {
            Ok(Mt6883Lane {
                lane_id: row.get(0)?,
                lane_name: row.get(1)?,
                domain: row.get(2)?,
                status: row.get(3)?,
                mt6883_ok: row.get::<_, i32>(4)? == 1,
                neuroethic_ok: row.get::<_, i32>(5)? == 1,
                neuroethic_radius_hours: row.get(6)?,
                capacity_pct: row.get(7)?,
                latency_ms: row.get(8)?,
                error_rate: row.get(9)?,
                continuity_status: row.get(10)?,
            })
        })?;

        rows.collect()
    }

    // -------------------------------------------------------------------------
    // Identity Binding Queries
    // -------------------------------------------------------------------------

    /// Get all identity bindings
    pub fn get_identity_bindings(&self) -> SqliteResult<Vec<IdentityBinding>> {
        let mut stmt = self.conn.prepare(
            "SELECT 
                binding_id, bostrom_address, logicalname, file_path,
                region, scope, dbrole, role_description
             FROM restoration_identity_binding
             ORDER BY dbrole, logicalname"
        )?;

        let rows = stmt.query_map([], |row| {
            Ok(IdentityBinding {
                binding_id: row.get(0)?,
                bostrom_address: row.get(1)?,
                logicalname: row.get(2)?,
                file_path: row.get(3)?,
                region: row.get(4)?,
                scope: row.get(5)?,
                dbrole: row.get(6)?,
                role_description: row.get(7)?,
            })
        })?;

        rows.collect()
    }

    /// Verify identity binding for a specific file
    pub fn verify_binding(&self, file_path: &str, bostrom_address: &str) -> SqliteResult<bool> {
        let mut stmt = self.conn.prepare(
            "SELECT COUNT(*) FROM restoration_identity_binding 
             WHERE file_path = ? AND bostrom_address = ?"
        )?;

        let count: i32 = stmt.query_row(params![file_path, bostrom_address], |row| row.get(0))?;
        Ok(count > 0)
    }

    // -------------------------------------------------------------------------
    // Definition Registry Queries
    // -------------------------------------------------------------------------

    /// Get all active definition registry entries
    pub fn get_active_definitions(&self) -> SqliteResult<Vec<DefinitionRegistryEntry>> {
        let mut stmt = self.conn.prepare(
            "SELECT 
                definition_id, logicalname, versiontag, artifact_type,
                file_path, status, region, scope, description
             FROM definitionregistry_restoration
             WHERE status IN ('ACTIVE', 'REGISTERED')
             ORDER BY artifact_type, logicalname"
        )?;

        let rows = stmt.query_map([], |row| {
            Ok(DefinitionRegistryEntry {
                definition_id: row.get(0)?,
                logicalname: row.get(1)?,
                versiontag: row.get(2)?,
                artifact_type: row.get(3)?,
                file_path: row.get(4)?,
                status: row.get(5)?,
                region: row.get(6)?,
                scope: row.get(7)?,
                description: row.get(8)?,
            })
        })?;

        rows.collect()
    }

    /// Check if a logicalname is registered
    pub fn is_logicalname_registered(&self, logicalname: &str) -> SqliteResult<bool> {
        let mut stmt = self.conn.prepare(
            "SELECT COUNT(*) FROM definitionregistry_restoration 
             WHERE logicalname = ? AND status IN ('ACTIVE', 'REGISTERED')"
        )?;

        let count: i32 = stmt.query_row(params![logicalname], |row| row.get(0))?;
        Ok(count > 0)
    }

    // -------------------------------------------------------------------------
    // Governance Summary Queries
    // -------------------------------------------------------------------------

    /// Get governance summary metrics
    pub fn get_governance_summary(&self) -> SqliteResult<Vec<GovernanceSummary>> {
        let mut stmt = self.conn.prepare(
            "SELECT 
                metric_category, total_count, approved_count, avg_score
             FROM v_agent_governance_summary_phx"
        )?;

        let rows = stmt.query_map([], |row| {
            Ok(GovernanceSummary {
                metric_category: row.get(0)?,
                total_count: row.get(1)?,
                approved_count: row.get(2)?,
                avg_score: row.get(3)?,
            })
        })?;

        rows.collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_author_constant() {
        assert_eq!(AUTHOR_BOSTROM, "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7");
    }

    #[test]
    fn test_default_db_path() {
        assert_eq!(DEFAULT_DB_PATH, "db/restorationindex.sqlite3");
    }
}
