//! ppx_continuity_kernel
//!
//! Neutral, sovereignty-safe continuity and neurorights evidence kernel for Prometheus-Praxis,
//! backed by the minimal SQLite schema you already defined:
//! - ppx_braindid
//! - ppx_psychstateref
//! - ppx_similaritymetric
//! - ppx_psychcontinuityevidence
//! - ppx_neuroright_corridorspec
//! - ppx_system
//! - ppx_systemwellbeingcomponent
//! - ppx_usercontinuitypreference
//! - ppx_sovereigntyguarantee
//!
//! Properties:
//! - Read-only JSON APIs suitable for MCP/agents.
//! - No actuation, no identity classification, no rights downgrade logic.
//! - Optional PPX no-identity-classification validator hook for CI/build.rs.

use rusqlite::{params, Connection};
use serde::{Deserialize, Serialize};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum KernelError {
    #[error("SQLite error: {0}")]
    Sqlite(#[from] rusqlite::Error),
    #[error("Serialization error: {0}")]
    Serde(#[from] serde_json::Error),
    #[error("Not found: {0}")]
    NotFound(String),
}

/// Mirror of `ppx_braindid` table.
/// Schema: did TEXT PRIMARY KEY, method TEXT NOT NULL, controller TEXT NOT NULL.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BrainDid {
    pub did: String,
    pub method: String,
    pub controller: String,
}

/// Mirror of `ppx_psychstateref` table.
/// Schema: id INTEGER PK, shardid TEXT NOT NULL, versiontag TEXT NOT NULL.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PsychStateRef {
    pub id: i64,
    pub shard_id: String,
    pub version_tag: String,
}

/// Mirror of `ppx_similaritymetric` table.
/// Schema: id TEXT PK, description TEXT NOT NULL.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SimilarityMetric {
    pub id: String,
    pub description: String,
}

/// Corresponds to the ALN `PsychContinuityEvidence` atom and
/// `ppx_psychcontinuityevidence` table:
/// evidenceid INTEGER PK,
/// subjectdid TEXT NOT NULL,
/// fromstateid INTEGER NOT NULL,
/// tostateid INTEGER NOT NULL,
/// metricid TEXT NOT NULL,
/// score REAL NOT NULL CHECK 0.0 <= score <= 1.0,
/// measuredatutc TEXT NOT NULL,
/// notes TEXT.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PsychContinuityEvidence {
    pub evidence_id: i64,
    pub subject_did: String,
    pub from_state_id: i64,
    pub to_state_id: i64,
    pub metric_id: String,
    pub score: f64,
    pub measured_at_utc: String,
    pub notes: Option<String>,
}

/// Mirror of `ppx_neuroright_corridorspec` table (NeurorightCorridorSpec):
/// id TEXT PRIMARY KEY,
/// contexttag TEXT NOT NULL,
/// description TEXT NOT NULL,
/// rightname TEXT NOT NULL,
/// minprotectionlevel REAL NOT NULL CHECK 0.0 <= .. <= 1.0,
/// maxrisktolerance REAL NOT NULL CHECK 0.0 <= .. <= 1.0.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NeurorightCorridorSpec {
    pub id: String,
    pub context_tag: String,
    pub description: String,
    pub right_name: String,
    pub min_protection_level: f64,
    pub max_risk_tolerance: f64,
}

/// Mirror of `ppx_system`:
/// systemid TEXT PRIMARY KEY, description TEXT NOT NULL.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct System {
    pub system_id: String,
    pub description: String,
}

/// Mirror of `ppx_systemwellbeingcomponent`:
/// id INTEGER PK,
/// systemid TEXT NOT NULL,
/// contexttag TEXT NOT NULL,
/// componentname TEXT NOT NULL,
/// value REAL NOT NULL CHECK 0.0 <= value <= 1.0,
/// description TEXT NOT NULL,
/// assessedatutc TEXT NOT NULL,
/// notes TEXT.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SystemWellBeingComponent {
    pub id: i64,
    pub system_id: String,
    pub context_tag: String,
    pub component_name: String,
    pub value: f64,
    pub description: String,
    pub assessed_at_utc: String,
    pub notes: Option<String>,
}

/// Mirror of `ppx_usercontinuitypreference` (UserContinuityPreference):
/// subjectdid TEXT PRIMARY KEY,
/// preferredmetricid TEXT NOT NULL,
/// preferredminscore REAL NOT NULL CHECK 0.0 <= .. <= 1.0,
/// note TEXT.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UserContinuityPreference {
    pub subject_did: String,
    pub preferred_metric_id: String,
    pub preferred_min_score: f64,
    pub note: Option<String>,
}

/// Sovereignty guarantees table:
/// id TEXT PRIMARY KEY, text TEXT NOT NULL.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SovereigntyGuarantee {
    pub id: String,
    pub text: String,
}

/// Lightweight handle for read-only operations.
#[derive(Debug)]
pub struct ContinuityKernel {
    conn: Connection,
}

impl ContinuityKernel {
    /// Open a read-only kernel on an existing SQLite database path.
    /// Callers should ensure the `db_ppx_minimal_continuity_neurorights.sql`
    /// schema has been applied beforehand.
    pub fn open_read_only(path: &str) -> Result<Self, KernelError> {
        let conn = Connection::open(path)?;
        // Enforce foreign keys even for read-only clients (no-op but safe).
        conn.pragma_update(None, "foreign_keys", "ON")?;
        Ok(Self { conn })
    }

    // -------------------------------------------------------------------------
    // BrainDid and UserContinuityPreference
    // -------------------------------------------------------------------------

    /// Fetch a `BrainDid` by DID string.
    pub fn get_braindid(&self, did: &str) -> Result<BrainDid, KernelError> {
        let mut stmt = self.conn.prepare(
            "SELECT did, method, controller FROM ppx_braindid WHERE did = ?1",
        )?;
        let opt = stmt
            .query_row(params![did], |row| {
                Ok(BrainDid {
                    did: row.get(0)?,
                    method: row.get(1)?,
                    controller: row.get(2)?,
                })
            })
            .optional()?;
        opt.ok_or_else(|| KernelError::NotFound(did.to_string()))
    }

    /// Fetch `UserContinuityPreference` for a given subject DID, if present.
    pub fn get_user_continuity_preference(
        &self,
        subject_did: &str,
    ) -> Result<Option<UserContinuityPreference>, KernelError> {
        let mut stmt = self.conn.prepare(
            "SELECT subjectdid, preferredmetricid, preferredminscore, note
             FROM ppx_usercontinuitypreference
             WHERE subjectdid = ?1",
        )?;
        let opt = stmt
            .query_row(params![subject_did], |row| {
                Ok(UserContinuityPreference {
                    subject_did: row.get(0)?,
                    preferred_metric_id: row.get(1)?,
                    preferred_min_score: row.get(2)?,
                    note: row.get(3)?,
                })
            })
            .optional()?;
        Ok(opt)
    }

    // -------------------------------------------------------------------------
    // Similarity metrics and psych state refs
    // -------------------------------------------------------------------------

    /// List all registered similarity metrics.
    pub fn list_similarity_metrics(&self) -> Result<Vec<SimilarityMetric>, KernelError> {
        let mut stmt = self.conn.prepare(
            "SELECT id, description FROM ppx_similaritymetric ORDER BY id",
        )?;
        let iter = stmt.query_map([], |row| {
            Ok(SimilarityMetric {
                id: row.get(0)?,
                description: row.get(1)?,
            })
        })?;
        let mut out = Vec::new();
        for item in iter {
            out.push(item?);
        }
        Ok(out)
    }

    /// Fetch a single psych state reference by ID.
    pub fn get_psych_state_ref(&self, id: i64) -> Result<PsychStateRef, KernelError> {
        let mut stmt = self.conn.prepare(
            "SELECT id, shardid, versiontag FROM ppx_psychstateref WHERE id = ?1",
        )?;
        let opt = stmt
            .query_row(params![id], |row| {
                Ok(PsychStateRef {
                    id: row.get(0)?,
                    shard_id: row.get(1)?,
                    version_tag: row.get(2)?,
                })
            })
            .optional()?;
        opt.ok_or_else(|| KernelError::NotFound(id.to_string()))
    }

    // -------------------------------------------------------------------------
    // PsychContinuityEvidence
    // -------------------------------------------------------------------------

    /// List continuity evidence for a subject DID within an optional time window.
    ///
    /// - `from_utc` and `to_utc` are optional RFC3339 strings; if `None`, the
    ///   bound is open.
    pub fn list_psych_continuity_evidence_for_subject(
        &self,
        subject_did: &str,
        from_utc: Option<&str>,
        to_utc: Option<&str>,
        limit: u32,
    ) -> Result<Vec<PsychContinuityEvidence>, KernelError> {
        let mut sql = String::from(
            "SELECT evidenceid, subjectdid, fromstateid, tostateid, metricid, \
             score, measuredatutc, notes \
             FROM ppx_psychcontinuityevidence \
             WHERE subjectdid = ?1",
        );
        if from_utc.is_some() {
            sql.push_str(" AND measuredatutc >= ?2");
        }
        if to_utc.is_some() {
            sql.push_str(" AND measuredatutc <= ?3");
        }
        sql.push_str(" ORDER BY measuredatutc DESC");
        if limit > 0 {
            sql.push_str(" LIMIT ");
            sql.push_str(&limit.to_string());
        }

        let mut stmt = self.conn.prepare(&sql)?;

        let mut params_vec: Vec<&dyn rusqlite::ToSql> = Vec::new();
        params_vec.push(&subject_did);
        if let Some(v) = from_utc {
            params_vec.push(&v);
        }
        if let Some(v) = to_utc {
            params_vec.push(&v);
        }

        let iter = stmt.query_map(&*params_vec, |row| {
            Ok(PsychContinuityEvidence {
                evidence_id: row.get(0)?,
                subject_did: row.get(1)?,
                from_state_id: row.get(2)?,
                to_state_id: row.get(3)?,
                metric_id: row.get(4)?,
                score: row.get(5)?,
                measured_at_utc: row.get(6)?,
                notes: row.get(7)?,
            })
        })?;

        let mut out = Vec::new();
        for item in iter {
            out.push(item?);
        }
        Ok(out)
    }

    // -------------------------------------------------------------------------
    // NeurorightCorridorSpec
    // -------------------------------------------------------------------------

    /// List all neuroright corridor specs for a given context tag.
    pub fn list_neuroright_corridors_for_context(
        &self,
        context_tag: &str,
    ) -> Result<Vec<NeurorightCorridorSpec>, KernelError> {
        let mut stmt = self.conn.prepare(
            "SELECT id, contexttag, description, rightname, \
             minprotectionlevel, maxrisktolerance \
             FROM ppx_neuroright_corridorspec \
             WHERE contexttag = ?1 \
             ORDER BY rightname",
        )?;
        let iter = stmt.query_map(params![context_tag], |row| {
            Ok(NeurorightCorridorSpec {
                id: row.get(0)?,
                context_tag: row.get(1)?,
                description: row.get(2)?,
                right_name: row.get(3)?,
                min_protection_level: row.get(4)?,
                max_risk_tolerance: row.get(5)?,
            })
        })?;
        let mut out = Vec::new();
        for item in iter {
            out.push(item?);
        }
        Ok(out)
    }

    /// Fetch a single neuroright corridor spec by ID.
    pub fn get_neuroright_corridor_by_id(
        &self,
        id: &str,
    ) -> Result<NeurorightCorridorSpec, KernelError> {
        let mut stmt = self.conn.prepare(
            "SELECT id, contexttag, description, rightname, \
             minprotectionlevel, maxrisktolerance \
             FROM ppx_neuroright_corridorspec \
             WHERE id = ?1",
        )?;
        let opt = stmt
            .query_row(params![id], |row| {
                Ok(NeurorightCorridorSpec {
                    id: row.get(0)?,
                    context_tag: row.get(1)?,
                    description: row.get(2)?,
                    right_name: row.get(3)?,
                    min_protection_level: row.get(4)?,
                    max_risk_tolerance: row.get(5)?,
                })
            })
            .optional()?;
        opt.ok_or_else(|| KernelError::NotFound(id.to_string()))
    }

    // -------------------------------------------------------------------------
    // System well-being
    // -------------------------------------------------------------------------

    /// List all well-being components for a system in a given context.
    pub fn list_system_wellbeing_components(
        &self,
        system_id: &str,
        context_tag: &str,
    ) -> Result<Vec<SystemWellBeingComponent>, KernelError> {
        let mut stmt = self.conn.prepare(
            "SELECT id, systemid, contexttag, componentname, value, \
             description, assessedatutc, notes \
             FROM ppx_systemwellbeingcomponent \
             WHERE systemid = ?1 AND contexttag = ?2 \
             ORDER BY assessedatutc DESC, componentname",
        )?;
        let iter = stmt.query_map(params![system_id, context_tag], |row| {
            Ok(SystemWellBeingComponent {
                id: row.get(0)?,
                system_id: row.get(1)?,
                context_tag: row.get(2)?,
                component_name: row.get(3)?,
                value: row.get(4)?,
                description: row.get(5)?,
                assessed_at_utc: row.get(6)?,
                notes: row.get(7)?,
            })
        })?;
        let mut out = Vec::new();
        for item in iter {
            out.push(item?);
        }
        Ok(out)
    }

    // -------------------------------------------------------------------------
    // Sovereignty guarantees
    // -------------------------------------------------------------------------

    /// List sovereignty guarantees; should include:
    /// - PPX-NO-IDENTITY-CLASSIFICATION
    /// - PPX-NO-RIGHTS-DOWNGRADE-BY-METRIC
    pub fn list_sovereignty_guarantees(&self) -> Result<Vec<SovereigntyGuarantee>, KernelError> {
        let mut stmt = self.conn.prepare(
            "SELECT id, text FROM ppx_sovereigntyguarantee ORDER BY id",
        )?;
        let iter = stmt.query_map([], |row| {
            Ok(SovereigntyGuarantee {
                id: row.get(0)?,
                text: row.get(1)?,
            })
        })?;
        let mut out = Vec::new();
        for item in iter {
            out.push(item?);
        }
        Ok(out)
    }

    // -------------------------------------------------------------------------
    // JSON helpers for MCP/agents
    // -------------------------------------------------------------------------

    /// Helper: serialize any serializable value to compact JSON.
    fn to_json<T: Serialize>(value: &T) -> Result<String, KernelError> {
        Ok(serde_json::to_string(value)?)
    }

    /// JSON API: list continuity evidence for an augmented citizen over a window.
    pub fn json_psych_continuity_for_subject(
        &self,
        subject_did: &str,
        from_utc: Option<&str>,
        to_utc: Option<&str>,
        limit: u32,
    ) -> Result<String, KernelError> {
        let evidence = self.list_psych_continuity_evidence_for_subject(subject_did, from_utc, to_utc, limit)?;
        Self::to_json(&evidence)
    }

    /// JSON API: obtain user continuity preference (if any) for a subject.
    pub fn json_user_continuity_preference(
        &self,
        subject_did: &str,
    ) -> Result<String, KernelError> {
        let pref = self.get_user_continuity_preference(subject_did)?;
        Self::to_json(&pref)
    }

    /// JSON API: list neuroright corridor specs for a context.
    pub fn json_neuroright_corridors_for_context(
        &self,
        context_tag: &str,
    ) -> Result<String, KernelError> {
        let specs = self.list_neuroright_corridors_for_context(context_tag)?;
        Self::to_json(&specs)
    }

    /// JSON API: list system well-being components for a system/context.
    pub fn json_system_wellbeing_components(
        &self,
        system_id: &str,
        context_tag: &str,
    ) -> Result<String, KernelError> {
        let comps = self.list_system_wellbeing_components(system_id, context_tag)?;
        Self::to_json(&comps)
    }

    /// JSON API: list sovereignty guarantees for CI/agents to inspect.
    pub fn json_sovereignty_guarantees(&self) -> Result<String, KernelError> {
        let g = self.list_sovereignty_guarantees()?;
        Self::to_json(&g)
    }
}

// -----------------------------------------------------------------------------
// Optional PPX validator hook
// -----------------------------------------------------------------------------

/// When compiled with the `ppx-validator` feature, this re-exports a convenience
/// function that can be called from a dependent crate's `build.rs` or CI binary
/// to run the PPX NO-IDENTITY-CLASSIFICATION invariant.
/// 
/// You already have `PpxNoIdentityClassificationValidator` and
/// `validate_ppx_invariants_or_panic` in your existing PPX crate. This simply
/// wraps that entry point so continuity-only crates can depend on it without
/// re-implementing the logic. [file:16]
#[cfg(feature = "ppx-validator")]
pub mod ppx_validator_hook {
    use super::KernelError;
    use aletheion_identity_ppx_no_identity_classification::{
        validate_ppx_invariants_or_panic, CrateSymbolProvider,
    };
    use std::path::Path;

    /// Run the PPX invariants for the continuity kernel crate.
    ///
    /// - `cfg_path` – path to the PPX JSON config exported from ALN
    ///   (e.g. `PPX-NO-IDENTITY-CLASSIFICATION-001.json`).
    /// - `provider` – crate symbol provider (call graph + symbol inventory)
    ///   supplied by your existing analysis pipeline.
    ///
    /// This function should be called only in build scripts or CI binaries,
    /// never at runtime in production binaries.
    pub fn run_ppx_no_identity_classification<P: CrateSymbolProvider>(
        cfg_path: &Path,
        provider: P,
    ) -> Result<(), KernelError> {
        // This will panic on violation; we wrap in Result for CI binaries that
        // prefer explicit error handling.
        validate_ppx_invariants_or_panic(cfg_path, provider);
        Ok(())
    }
}

// -----------------------------------------------------------------------------
// rusqlite helper: optional() until it lands in your version.
// -----------------------------------------------------------------------------

trait OptionalRow<T> {
    fn optional(self) -> Result<Option<T>, rusqlite::Error>;
}

impl<T> OptionalRow<T> for Result<T, rusqlite::Error> {
    fn optional(self) -> Result<Option<T>, rusqlite::Error> {
        match self {
            Ok(v) => Ok(Some(v)),
            Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
            Err(e) => Err(e),
        }
    }
}
