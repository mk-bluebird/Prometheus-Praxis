// filename: crates/econet_ecowealth/src/daily_manifest.rs
// destination: eco_restoration_shard/crates/econet_ecowealth/src/daily_manifest.rs
// description: Non-actuating Rust query harness for validating and parsing the 
// Phoenix Daily Evolution Manifest from the local SQLite index.

use rusqlite::{Connection, Result, Row};
use serde::{Deserialize, Serialize};

/// Represents a single steward's performance over a daily window.
/// Enforces adherence to KER > 0.90, E > 0.90, R < 0.13 conceptually via struct rules.
#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct DailyEvolutionManifestRecord {
    pub steward_did: String,
    pub region_code: String,
    pub day: String,
    
    // Core KER residual fields
    pub k_mean_day: f64,
    pub e_mean_day: f64,
    pub r_mean_day: f64,
    pub vt_max_day: f64,
    pub reff_mean_day: f64,
    
    // Ecowealth outputs
    pub ecounit_issued_day: f64,
    pub ecounits_credited_day: f64,
    
    // Risk & Responsibility Overlays
    pub roh_max_day: f64,
    pub roh_ok_day: u8,
    pub rresponsibility_day: f64,
    pub rpharma_day: f64,
    pub rtoxicity_day: f64,
    pub roverride_day: f64,
    
    // Biological metrics
    pub lifeforce_delta_day: f64,
    pub biokarma_delta_day: f64,
    
    // Governance and decentralization protections
    pub governancepenalty_day: f64,
    pub dataqualityfactor_day: f64,
    pub representationfloor: f64,
    pub actual_ecowealth_share_day: f64,
    pub representation_ok: u8,
    pub evolution_eligible_day: u8,
}

impl DailyEvolutionManifestRecord {
    /// Validates record strictly against mathematical thresholds derived from ALN proofs.
    pub fn is_mathematically_admissible(&self) -> bool {
        // Enforce K>=0.90, E>=0.90, R<=0.13, RoH<=0.30 via strict Rust 2024 logical binding.
        self.k_mean_day >= 0.90
            && self.e_mean_day >= 0.90
            && self.r_mean_day <= 0.13
            && self.roh_max_day <= 0.30
            && self.roh_ok_day == 1
            && self.evolution_eligible_day == 1
    }
}

/// Provides purely read-only (non-actuating) access to the pre-wired SQL manifest.
pub struct ManifestInspector {
    db_path: std::path::PathBuf,
}

impl ManifestInspector {
    pub fn new<P: AsRef<std::path::Path>>(path: P) -> Self {
        Self {
            db_path: path.as_ref().to_path_buf(),
        }
    }

    /// Fetches the manifest snapshot for a specific DID and window.
    pub fn fetch_steward_manifest(
        &self,
        steward_did: &str,
        date_iso: &str,
    ) -> Result<Option<DailyEvolutionManifestRecord>> {
        let conn = Connection::open(&self.db_path)?;

        let mut stmt = conn.prepare(
            r#"
            SELECT 
                stewarddid, regioncode, day, k_mean_day, e_mean_day, r_mean_day, 
                vt_max_day, reff_mean_day, ecounit_issued_day, ecounits_credited_day,
                roh_max_day, roh_ok_day, rresponsibility_day, rpharma_day,
                rtoxicity_day, roverride_day, lifeforce_delta_day, biokarma_delta_day,
                governancepenalty_day, dataqualityfactor_day, representationfloor,
                actual_ecowealth_share_day, representation_ok, evolution_eligible_day
            FROM vphx_daily_evolution_manifest
            WHERE stewarddid = ?1 AND day = ?2
            "#
        )?;

        let mut iter = stmt.query_map([steward_did, date_iso], Self::map_row)?;
        
        if let Some(record) = iter.next() {
            return Ok(Some(record?));
        }
        
        Ok(None)
    }

    fn map_row(row: &Row) -> Result<DailyEvolutionManifestRecord> {
        Ok(DailyEvolutionManifestRecord {
            steward_did: row.get(0)?,
            region_code: row.get(1)?,
            day: row.get(2)?,
            k_mean_day: row.get(3)?,
            e_mean_day: row.get(4)?,
            r_mean_day: row.get(5)?,
            vt_max_day: row.get(6)?,
            reff_mean_day: row.get(7)?,
            ecounit_issued_day: row.get(8)?,
            ecounits_credited_day: row.get(9)?,
            roh_max_day: row.get(10)?,
            roh_ok_day: row.get(11)?,
            rresponsibility_day: row.get(12)?,
            rpharma_day: row.get(13)?,
            rtoxicity_day: row.get(14)?,
            roverride_day: row.get(15)?,
            lifeforce_delta_day: row.get(16)?,
            biokarma_delta_day: row.get(17)?,
            governancepenalty_day: row.get(18)?,
            dataqualityfactor_day: row.get(19)?,
            representationfloor: row.get(20)?,
            actual_ecowealth_share_day: row.get(21)?,
            representation_ok: row.get(22)?,
            evolution_eligible_day: row.get(23)?,
        })
    }
}
