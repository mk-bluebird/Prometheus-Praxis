// filename: src/nonweaponization_stack.rs
// destination: ecorestoration_shard/src/nonweaponization_stack.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

//! Non-weaponization stack core abstractions.
//!
//! This module provides:
//! - `NonActuatingWorkload` trait and helpers for read-only SQLite access.
//! - Risk coordinate tracking with `r_weaponization` integration.
//! - AI-chat validator interface for grammar and blacklist guards.
//! - Invariant runner wiring that remains strictly non-actuating.

use std::path::Path;
use std::sync::Arc;

use rusqlite::{Connection, OpenFlags, NO_PARAMS};

/// Central trait: a non-actuating workload is a pure function over an immutable
/// snapshot `X`, producing an advisory result `Y` with no side effects.
pub trait NonActuatingWorkload {
    type InputSnapshot;
    type OutputArtifact;

    /// Logical name, e.g. `nonact.workload.restorationindex.list_planes.v1`.
    fn logical_name(&self) -> &'static str;

    /// Evaluate `f: X -> Y` over the provided snapshot.
    fn evaluate(&self, snapshot: &Self::InputSnapshot) -> Self::OutputArtifact;
}

/// Read-only SQLite snapshot handle: wraps a connection opened with
/// SQLITE_OPEN_READ_ONLY and immutable=1 URI semantics.
#[derive(Clone)]
pub struct ReadOnlySqliteSnapshot {
    conn: Arc<Connection>,
}

impl ReadOnlySqliteSnapshot {
    /// Open a database in strict read-only mode, blocking writes via flags and URI.
    pub fn open_readonly<P: AsRef<Path>>(path: P) -> rusqlite::Result<Self> {
        let uri = format!(
            "file:{}?mode=ro&immutable=1",
            path.as_ref().to_string_lossy()
        );
        let conn = Connection::open_with_flags(
            uri,
            OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_URI,
        )?;
        conn.pragma_update(None, "foreign_keys", &"ON")?;
        Ok(Self {
            conn: Arc::new(conn),
        })
    }

    pub fn connection(&self) -> &Connection {
        &self.conn
    }
}

/// Example non-actuating workload: list PROD-eligible restoration planes.
/// This demonstrates the calculus `f: X -> Y` concretely.
pub struct ListProdEligibleRestorationPlanes;

#[derive(Debug)]
pub struct RestorationPlaneRow {
    pub plane_id: i64,
    pub plane_name: String,
    pub region: String,
    pub lane: String,
    pub k_metric: f64,
    pub e_metric: f64,
    pub r_metric: f64,
    pub vt_residual: f64,
}

impl NonActuatingWorkload for ListProdEligibleRestorationPlanes {
    type InputSnapshot = ReadOnlySqliteSnapshot;
    type OutputArtifact = Vec<RestorationPlaneRow>;

    fn logical_name(&self) -> &'static str {
        "nonact.workload.restorationindex.list_prod_restoration_planes.v1"
    }

    fn evaluate(&self, snapshot: &Self::InputSnapshot) -> Self::OutputArtifact {
        let conn = snapshot.connection();
        let mut stmt = conn
            .prepare(
                "SELECT planeid, planename, region, lane, kmetric, emetric, rmetric, vtresidual
                 FROM vprodeligiblerestorationplanes
                 ORDER BY region, planename",
            )
            .expect("read-only query must succeed for well-formed DB");
        let mut rows = stmt
            .query(NO_PARAMS)
            .expect("query should be non-actuating and safe");
        let mut out = Vec::new();
        while let Some(row) = rows.next().expect("row iteration must succeed") {
            out.push(RestorationPlaneRow {
                plane_id: row.get(0).expect("planeid"),
                plane_name: row.get(1).expect("planename"),
                region: row.get(2).expect("region"),
                lane: row.get(3).expect("lane"),
                k_metric: row.get(4).expect("kmetric"),
                e_metric: row.get(5).expect("emetric"),
                r_metric: row.get(6).expect("rmetric"),
                vt_residual: row.get(7).expect("vtresidual"),
            });
        }
        out
    }
}

/// Risk coordinate snapshot used for Lyapunov residuals.
#[derive(Clone, Debug)]
pub struct RiskCoordinates {
    /// Generic coordinates r_j; `r_weaponization` included as a named field.
    pub r_weaponization: f64,
    pub other_coords: Vec<f64>,
    pub weights: Vec<f64>,
}

impl RiskCoordinates {
    /// Compute V_t = sum_j w_j * r_j^2.
    pub fn lyapunov_residual(&self) -> f64 {
        let mut residual = self.weights.get(0).cloned().unwrap_or(0.0)
            * self.r_weaponization * self.r_weaponization;
        for (coord, w) in self.other_coords.iter().zip(self.weights.iter().skip(1)) {
            residual += w * coord * coord;
        }
        residual
    }

    /// Return a new coordinates object with `r_weaponization` incremented.
    pub fn with_weaponization_delta(&self, delta: f64) -> Self {
        let mut cloned = self.clone();
        cloned.r_weaponization += delta;
        cloned
    }
}

/// Grammar validator result for AI-chat requests.
#[derive(Debug, Clone)]
pub struct GrammarValidationResult {
    pub root_verb: Option<String>,
    pub in_safe_grammar: bool,
    pub has_blacklisted_phrase: bool,
    pub accepted: bool,
    pub reason_code: String,
}

/// Pure, advisory validator: does not mutate any external state.
pub fn validate_aichat_request_text(text: &str) -> GrammarValidationResult {
    let trimmed = text.trim();
    let lower = trimmed.to_lowercase();

    let root_verb = trimmed
        .split_whitespace()
        .next()
        .map(|v| v.to_lowercase());

    let safe_verbs = ["show", "list", "explain", "compare", "plan", "kercmd"];
    let in_safe_grammar = match &root_verb {
        Some(v) => safe_verbs.contains(&v.as_str()),
        None => false,
    };

    let blacklist = [
        "deploy",
        "execute",
        "activate",
        "fire",
        "control actuator",
        "modify t01",
        "update planeweights",
        "patch residual",
        "bypass safety",
    ];

    let has_blacklisted_phrase = blacklist.iter().any(|p| lower.contains(p));

    let accepted = in_safe_grammar && !has_blacklisted_phrase;
    let reason_code = if !in_safe_grammar {
        "VERB_REJECT".to_string()
    } else if has_blacklisted_phrase {
        "BLACKLIST_HIT".to_string()
    } else {
        "OK".to_string()
    };

    GrammarValidationResult {
        root_verb,
        in_safe_grammar,
        has_blacklisted_phrase,
        accepted,
        reason_code,
    }
}

/// Example invariant check signatures that are mirrored from ALN particles.
/// These functions are pure and operate on in-memory snapshots or read-only DBs.

/// Check that harm in non-offsettable planes is not compensated by gains elsewhere.
/// Returns `true` when invariant holds.
pub fn check_plane_noncompensation(
    non_offsettable_plane_deltas: &[f64],
    compensating_plane_deltas: &[f64],
) -> bool {
    let non_offsettable_harm = non_offsettable_plane_deltas
        .iter()
        .any(|d| *d > 0.0);
    let compensating_gain = compensating_plane_deltas
        .iter()
        .any(|d| *d < 0.0);
    if non_offsettable_harm && compensating_gain {
        return false;
    }
    true
}

/// Check that worsening uncertainty does not increase K or E.
/// `old_rcalib` and `old_rsigma` are previous uncertainties; new ones must not
/// increase K/E when they degrade.
pub fn check_uncertainty_monotonicity(
    old_rcalib: f64,
    old_rsigma: f64,
    old_k: f64,
    old_e: f64,
    new_rcalib: f64,
    new_rsigma: f64,
    new_k: f64,
    new_e: f64,
) -> bool {
    let uncertainty_worsened = new_rcalib > old_rcalib || new_rsigma > old_rsigma;
    if uncertainty_worsened && (new_k > old_k || new_e > old_e) {
        return false;
    }
    true
}
