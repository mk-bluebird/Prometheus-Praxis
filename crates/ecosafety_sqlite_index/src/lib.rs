// Filename: crates/ecosafety_sqlite_index/src/lib.rs
// Target repo: github.com/Doctor0Evil/eco_restoration_shard (new crate under /crates)
// Purpose: Production-grade SQLite index + blast-radius tables for Cyboquatic industrial machinery
//          within the EcoNet constellation, wired to rx/Vt/KER grammar and Bostrom DIDs.
//
// Hard constraints respected:
// - Rust only, no Python, no blacklisted hashes, no hidden control.
// - Non‑actuating: this crate indexes and scores; it never touches hardware.
// - Every table row carries K, E, R, and an eco_risk flag to bound harm.

use std::path::Path;
use std::time::{SystemTime, UNIX_EPOCH};

use rusqlite::{params, Connection, OptionalExtension, Result as SqlResult};
use serde::{Deserialize, Serialize};

/// Normalized 0–1 risk coordinate, aligned with existing ecosafety grammar.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct RiskCoord(pub f64);

impl RiskCoord {
    pub fn new_clamped(v: f64) -> Self {
        Self(v.max(0.0).min(1.0))
    }

    pub fn value(self) -> f64 {
        self.0
    }
}

/// Unified KER triad per indexed object, consistent with rx/Vt/KER spine.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct KerTriad {
    /// Knowledge-factor: fraction of steps or design states respecting Lyapunov and corridors.
    pub k: f64,
    /// Eco-impact value: typically 1 - max_risk across relevant planes.
    pub e: f64,
    /// Risk-of-harm: max rx over evaluation window.
    pub r: f64,
}

impl KerTriad {
    pub fn new(k: f64, e: f64, r: f64) -> Self {
        Self { k, e, r }
    }

    /// Returns true if this triad is eligible for production lanes under K/E/R gates.
    pub fn is_prod_eligible(&self) -> bool {
        self.k >= 0.90 && self.e >= 0.90 && self.r <= 0.13
    }

    /// Simple eco-risk score in 0–1, higher = more harmful.
    /// This can be refined later, but must remain monotone in R.
    pub fn eco_risk_score(&self) -> f64 {
        // Weight residual risk more strongly than K/E deficits.
        let k_def = (0.90 - self.k).max(0.0);
        let e_def = (0.90 - self.e).max(0.0);
        let r_term = (self.r - 0.10).max(0.0); // 0.10 as aspirational band
        let raw = 0.4 * r_term + 0.3 * k_def + 0.3 * e_def;
        raw.max(0.0).min(1.0)
    }
}

/// Blast-radius classification for any change or deployment decision.
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub enum BlastRadiusClass {
    Low,
    Medium,
    High,
}

impl BlastRadiusClass {
    pub fn as_str(&self) -> &'static str {
        match self {
            BlastRadiusClass::Low => "LOW",
            BlastRadiusClass::Medium => "MEDIUM",
            BlastRadiusClass::High => "HIGH",
        }
    }

    pub fn from_str(s: &str) -> Option<Self> {
        match s {
            "LOW" => Some(BlastRadiusClass::Low),
            "MEDIUM" => Some(BlastRadiusClass::Medium),
            "HIGH" => Some(BlastRadiusClass::High),
            _ => None,
        }
    }
}

/// Minimal hex-stamped Bostrom identity anchor for repos/crates/assets.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BostromIdentity {
    /// Primary or alternate Bostrom address (hex / bech32-like string).
    pub address: String,
    /// Optional label e.g. "bostrom18sd2u… (primary)".
    pub label: String,
}

/// Core Cyboquatic node indexed in the SQLite constellation.
/// Non-actuating: describes machinery, corridors, and ecosafety envelope.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CyboNodeIndex {
    pub id: i64,
    pub node_key: String,          // stable key (e.g. "PHX-MAR-VAULT-001")
    pub node_type: String,         // e.g. "MAR_VAULT", "FOG_DESICCATOR", "BIO_SOFTBOT"
    pub basin: String,             // e.g. "PHX_BASIN_01"
    pub repo_hint: String,         // e.g. "eco_restoration_shard", "EcoNet"
    pub aln_contract: String,      // e.g. "CyboquaticNode2026v1.aln"
    pub corridor_set: String,      // e.g. "ecosafety.corridors.v2"
    pub ker_k: f64,
    pub ker_e: f64,
    pub ker_r: f64,
    pub eco_risk: f64,             // derived from KER
    pub carbon_plane_present: bool,
    pub biodiversity_plane_present: bool,
    pub materials_plane_present: bool,
    pub created_ts: i64,
    pub updated_ts: i64,
}

/// A single blast-radius entry describing a potential change or deployment.
/// This table lets CI reason about risk and automatically downgrade or block.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BlastRadiusEntry {
    pub id: i64,
    pub node_id: i64,
    pub change_key: String,        // e.g. "upgrade_corridors_v2.1", "new_material_blend_X"
    pub description: String,
    pub code_repo: String,         // target repo for change
    pub file_path: String,         // path of relevant spec/code file
    pub aln_contract: String,      // contract that will govern post-change
    pub ker_before_k: f64,
    pub ker_before_e: f64,
    pub ker_before_r: f64,
    pub ker_after_k: f64,
    pub ker_after_e: f64,
    pub ker_after_r: f64,
    pub eco_risk_before: f64,
    pub eco_risk_after: f64,
    pub blast_radius_class: BlastRadiusClass,
    pub evidence_hex: String,      // hex string of canonical change evidence
    pub bostrom_address: String,   // Bostrom DID for signer
    pub created_ts: i64,
}

/// Main wrapper around SQLite connection with schema bootstrap and helpers.
pub struct EcoNetIndex {
    conn: Connection,
}

impl EcoNetIndex {
    /// Open or create the constellation index at the given path.
    /// Path is determined by the orchestrator using indexed tables from EcoNet.
    pub fn open_or_create<P: AsRef<Path>>(path: P) -> SqlResult<Self> {
        let conn = Connection::open(path)?;
        let idx = Self { conn };
        idx.init_schema()?;
        Ok(idx)
    }

    fn init_schema(&self) -> SqlResult<()> {
        // Core node table, aligns with rx/Vt/KER grammar and planes (carbon, materials, biodiversity).
        self.conn.execute_batch(
            r#"
            PRAGMA foreign_keys = ON;

            CREATE TABLE IF NOT EXISTS cybo_nodes (
                id                        INTEGER PRIMARY KEY AUTOINCREMENT,
                node_key                  TEXT NOT NULL UNIQUE,
                node_type                 TEXT NOT NULL,
                basin                     TEXT NOT NULL,
                repo_hint                 TEXT NOT NULL,
                aln_contract              TEXT NOT NULL,
                corridor_set              TEXT NOT NULL,
                ker_k                     REAL NOT NULL,
                ker_e                     REAL NOT NULL,
                ker_r                     REAL NOT NULL,
                eco_risk                  REAL NOT NULL,
                carbon_plane_present      INTEGER NOT NULL, -- 0/1
                biodiversity_plane_present INTEGER NOT NULL,
                materials_plane_present   INTEGER NOT NULL,
                created_ts                INTEGER NOT NULL,
                updated_ts                INTEGER NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_cybo_nodes_basin
                ON cybo_nodes (basin);

            CREATE INDEX IF NOT EXISTS idx_cybo_nodes_repo
                ON cybo_nodes (repo_hint);

            CREATE INDEX IF NOT EXISTS idx_cybo_nodes_ker
                ON cybo_nodes (ker_k, ker_e, ker_r);

            CREATE INDEX IF NOT EXISTS idx_cybo_nodes_eco_risk
                ON cybo_nodes (eco_risk);

            -- Blast-radius table for changes spanning repos and contracts.
            CREATE TABLE IF NOT EXISTS blast_radius (
                id                   INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id              INTEGER NOT NULL REFERENCES cybo_nodes(id) ON DELETE CASCADE,
                change_key           TEXT NOT NULL,
                description          TEXT NOT NULL,
                code_repo            TEXT NOT NULL,
                file_path            TEXT NOT NULL,
                aln_contract         TEXT NOT NULL,
                ker_before_k         REAL NOT NULL,
                ker_before_e         REAL NOT NULL,
                ker_before_r         REAL NOT NULL,
                ker_after_k          REAL NOT NULL,
                ker_after_e          REAL NOT NULL,
                ker_after_r          REAL NOT NULL,
                eco_risk_before      REAL NOT NULL,
                eco_risk_after       REAL NOT NULL,
                blast_radius_class   TEXT NOT NULL,
                evidence_hex         TEXT NOT NULL,
                bostrom_address      TEXT NOT NULL,
                created_ts           INTEGER NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_blast_radius_node
                ON blast_radius (node_id);

            CREATE INDEX IF NOT EXISTS idx_blast_radius_change
                ON blast_radius (change_key);

            CREATE INDEX IF NOT EXISTS idx_blast_radius_class
                ON blast_radius (blast_radius_class);
            "#,
        )?;
        Ok(())
    }

    fn now_ts() -> i64 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs() as i64
    }

    /// Insert or update a Cyboquatic node index row.
    /// This function automatically computes eco_risk from the KER triad.
    pub fn upsert_node(
        &self,
        node_key: &str,
        node_type: &str,
        basin: &str,
        repo_hint: &str,
        aln_contract: &str,
        corridor_set: &str,
        ker: KerTriad,
        carbon_plane_present: bool,
        biodiversity_plane_present: bool,
        materials_plane_present: bool,
    ) -> SqlResult<i64> {
        let eco_risk = ker.eco_risk_score();
        let ts = Self::now_ts();

        // Try update first; if no row, insert.
        let updated = self.conn.execute(
            r#"
            UPDATE cybo_nodes
               SET node_type = ?2,
                   basin = ?3,
                   repo_hint = ?4,
                   aln_contract = ?5,
                   corridor_set = ?6,
                   ker_k = ?7,
                   ker_e = ?8,
                   ker_r = ?9,
                   eco_risk = ?10,
                   carbon_plane_present = ?11,
                   biodiversity_plane_present = ?12,
                   materials_plane_present = ?13,
                   updated_ts = ?14
             WHERE node_key = ?1
            "#,
            params![
                node_key,
                node_type,
                basin,
                repo_hint,
                aln_contract,
                corridor_set,
                ker.k,
                ker.e,
                ker.r,
                eco_risk,
                if carbon_plane_present { 1 } else { 0 },
                if biodiversity_plane_present { 1 } else { 0 },
                if materials_plane_present { 1 } else { 0 },
                ts
            ],
        )?;

        if updated == 0 {
            self.conn.execute(
                r#"
                INSERT INTO cybo_nodes (
                    node_key, node_type, basin, repo_hint,
                    aln_contract, corridor_set,
                    ker_k, ker_e, ker_r, eco_risk,
                    carbon_plane_present,
                    biodiversity_plane_present,
                    materials_plane_present,
                    created_ts, updated_ts
                ) VALUES (?1, ?2, ?3, ?4, ?5, ?6,
                          ?7, ?8, ?9, ?10,
                          ?11, ?12, ?13,
                          ?14, ?14)
                "#,
                params![
                    node_key,
                    node_type,
                    basin,
                    repo_hint,
                    aln_contract,
                    corridor_set,
                    ker.k,
                    ker.e,
                    ker.r,
                    eco_risk,
                    if carbon_plane_present { 1 } else { 0 },
                    if biodiversity_plane_present { 1 } else { 0 },
                    if materials_plane_present { 1 } else { 0 },
                    ts
                ],
            )?;
        }

        let id: i64 = self
            .conn
            .query_row(
                "SELECT id FROM cybo_nodes WHERE node_key = ?1",
                params![node_key],
                |row| row.get(0),
            )?;
        Ok(id)
    }

    /// Fetch a node by node_key.
    pub fn get_node(&self, node_key: &str) -> SqlResult<Option<CyboNodeIndex>> {
        self.conn
            .query_row(
                r#"
                SELECT id, node_key, node_type, basin, repo_hint,
                       aln_contract, corridor_set,
                       ker_k, ker_e, ker_r, eco_risk,
                       carbon_plane_present,
                       biodiversity_plane_present,
                       materials_plane_present,
                       created_ts, updated_ts
                  FROM cybo_nodes
                 WHERE node_key = ?1
                "#,
                params![node_key],
                |row| {
                    Ok(CyboNodeIndex {
                        id: row.get(0)?,
                        node_key: row.get(1)?,
                        node_type: row.get(2)?,
                        basin: row.get(3)?,
                        repo_hint: row.get(4)?,
                        aln_contract: row.get(5)?,
                        corridor_set: row.get(6)?,
                        ker_k: row.get(7)?,
                        ker_e: row.get(8)?,
                        ker_r: row.get(9)?,
                        eco_risk: row.get(10)?,
                        carbon_plane_present: row.get::<_, i64>(11)? != 0,
                        biodiversity_plane_present: row.get::<_, i64>(12)? != 0,
                        materials_plane_present: row.get::<_, i64>(13)? != 0,
                        created_ts: row.get(14)?,
                        updated_ts: row.get(15)?,
                    })
                },
            )
            .optional()
    }

    /// Insert a blast-radius record for a planned change across repos/contracts.
    /// evidence_hex must be computed by the caller using the canonical ProvenanceKernel.
    pub fn insert_blast_radius(
        &self,
        node_id: i64,
        change_key: &str,
        description: &str,
        code_repo: &str,
        file_path: &str,
        aln_contract: &str,
        ker_before: KerTriad,
        ker_after: KerTriad,
        evidence_hex: &str,
        bostrom: &BostromIdentity,
    ) -> SqlResult<i64> {
        let eco_before = ker_before.eco_risk_score();
        let eco_after = ker_after.eco_risk_score();
        let class = classify_blast_radius(eco_before, eco_after).as_str();
        let ts = Self::now_ts();

        self.conn.execute(
            r#"
            INSERT INTO blast_radius (
                node_id,
                change_key,
                description,
                code_repo,
                file_path,
                aln_contract,
                ker_before_k,
                ker_before_e,
                ker_before_r,
                ker_after_k,
                ker_after_e,
                ker_after_r,
                eco_risk_before,
                eco_risk_after,
                blast_radius_class,
                evidence_hex,
                bostrom_address,
                created_ts
            ) VALUES (?1, ?2, ?3, ?4, ?5, ?6,
                      ?7, ?8, ?9,
                      ?10, ?11, ?12,
                      ?13, ?14, ?15,
                      ?16, ?17, ?18)
            "#,
            params![
                node_id,
                change_key,
                description,
                code_repo,
                file_path,
                aln_contract,
                ker_before.k,
                ker_before.e,
                ker_before.r,
                ker_after.k,
                ker_after.e,
                ker_after.r,
                eco_before,
                eco_after,
                class,
                evidence_hex,
                bostrom.address,
                ts
            ],
        )?;

        let id: i64 = self.conn.query_row(
            "SELECT id FROM blast_radius WHERE node_id = ?1 AND change_key = ?2 ORDER BY id DESC LIMIT 1",
            params![node_id, change_key],
            |row| row.get(0),
        )?;
        Ok(id)
    }

    /// Query high-risk blast-radius entries for audit and CI gating.
    pub fn list_high_risk_changes(&self) -> SqlResult<Vec<BlastRadiusEntry>> {
        let mut stmt = self.conn.prepare(
            r#"
            SELECT id, node_id, change_key, description, code_repo,
                   file_path, aln_contract,
                   ker_before_k, ker_before_e, ker_before_r,
                   ker_after_k, ker_after_e, ker_after_r,
                   eco_risk_before, eco_risk_after,
                   blast_radius_class, evidence_hex,
                   bostrom_address, created_ts
              FROM blast_radius
             WHERE blast_radius_class = 'HIGH'
             ORDER BY created_ts DESC
            "#,
        )?;

        let rows = stmt.query_map([], |row| {
            Ok(BlastRadiusEntry {
                id: row.get(0)?,
                node_id: row.get(1)?,
                change_key: row.get(2)?,
                description: row.get(3)?,
                code_repo: row.get(4)?,
                file_path: row.get(5)?,
                aln_contract: row.get(6)?,
                ker_before_k: row.get(7)?,
                ker_before_e: row.get(8)?,
                ker_before_r: row.get(9)?,
                ker_after_k: row.get(10)?,
                ker_after_e: row.get(11)?,
                ker_after_r: row.get(12)?,
                eco_risk_before: row.get(13)?,
                eco_risk_after: row.get(14)?,
                blast_radius_class: BlastRadiusClass::from_str(&row.get::<_, String>(15)?)
                    .unwrap_or(BlastRadiusClass::High),
                evidence_hex: row.get(16)?,
                bostrom_address: row.get(17)?,
                created_ts: row.get(18)?,
            })
        })?;

        let mut out = Vec::new();
        for r in rows {
            out.push(r?);
        }
        Ok(out)
    }
}

/// Classify a change's blast radius from KER-based eco-risk before/after.
pub fn classify_blast_radius(
    eco_risk_before: f64,
    eco_risk_after: f64,
) -> BlastRadiusClass {
    let delta = eco_risk_after - eco_risk_before;
    // Three buckets, tuned to corridor-tightening agenda:
    // - LOW: eco risk stays low and does not increase significantly.
    // - MEDIUM: moderate risk or modest increase.
    // - HIGH: risk above 0.5 or significant increase.
    if eco_risk_after <= 0.25 && delta <= 0.05 {
        BlastRadiusClass::Low
    } else if eco_risk_after <= 0.5 && delta <= 0.15 {
        BlastRadiusClass::Medium
    } else {
        BlastRadiusClass::High
    }
}

// ---- Example usage wiring (non-actuating, for tests / documentation) ----

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn basic_upsert_and_blast_radius() {
        let tmp = tempfile::NamedTempFile::new().expect("tmp file");
        let idx = EcoNetIndex::open_or_create(tmp.path()).expect("open index");

        let ker_before = KerTriad::new(0.93, 0.92, 0.14);
        let node_id = idx
            .upsert_node(
                "PHX-MAR-VAULT-001",
                "MAR_VAULT",
                "PHX_BASIN_01",
                "eco_restoration_shard",
                "PhoenixMarNode2026v1.aln",
                "ecosafety.corridors.v2",
                ker_before,
                true,
                true,
                true,
            )
            .expect("upsert node");

        let ker_after = KerTriad::new(0.94, 0.93, 0.12);
        let bostrom = BostromIdentity {
            address: "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7".to_string(),
            label: "primary".to_string(),
        };

        let evidence_hex = "deadbeefcafebabe"; // placeholder; real code must use ProvenanceKernel2026v1

        let change_id = idx
            .insert_blast_radius(
                node_id,
                "tighten_PFAS_corridor_v2.1",
                "Tighten PFAS corridor based on Phoenix lab data; expect R to drop.",
                "EcoNet",
                "aln/ecosafety.corridors.PFAS.v2.1.aln",
                "PhoenixMarNode2026v1.aln",
                ker_before,
                ker_after,
                evidence_hex,
                &bostrom,
            )
            .expect("insert blast radius");

        assert!(change_id > 0);

        let highs = idx.list_high_risk_changes().expect("list highs");
        // This specific change should be Medium risk under default thresholds, so no highs expected.
        assert!(highs.is_empty());
    }
}
