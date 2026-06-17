// FILE: crates/ker-score-lab/src/lib.rs
// DESTINATION: crates/ker-score-lab/src/lib.rs
// REPO-TARGET: github.com/mk-bluebird/eco_restoration_shard
//
// Synthetic KER lab — fixture management, scoring, corridor-band checks,
// and first-class qpudatashard publication.
//
// Design:
//  - `KerScoreShard` is the publishable score unit.
//  - `CorridorBand` defines per-lane K/E/R corridor floors/ceilings.
//  - `replay_and_score` takes a sequence of KerPoint transitions, runs
//    all invariant checks, and produces a `KerScoreShard`.
//  - Blocking predicate: K drops, E drops, or R rises beyond corridor.

#![forbid(unsafe_code)]
#![warn(missing_docs)]

use chrono::Utc;
use serde::{Deserialize, Serialize};
use thiserror::Error;

use kerdeployable::{check_all_invariants, KerPoint};

/// Score-lab errors.
#[derive(Debug, Error)]
pub enum ScoreLabError {
    /// KER corridor breach — merge should be blocked.
    #[error("corridor breach: {0}")]
    CorridorBreach(String),
    /// Empty fixture sequence.
    #[error("fixture sequence is empty")]
    EmptyFixture,
    /// JSON error.
    #[error("json: {0}")]
    Json(#[from] serde_json::Error),
    /// DB error.
    #[error("db: {0}")]
    Db(#[from] rusqlite::Error),
}

/// KER corridor band for a named lane.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CorridorBand {
    /// Lane label, e.g. `"RESEARCH"`, `"PROD"`.
    pub lane: String,
    /// Minimum K required for this lane.
    pub k_floor: f64,
    /// Minimum E required for this lane.
    pub e_floor: f64,
    /// Maximum R permitted in this lane.
    pub r_ceiling: f64,
    /// Lyapunov slack ε for safe-step checks.
    pub vt_epsilon: f64,
}

impl CorridorBand {
    /// Returns `true` when all KER values satisfy this corridor.
    pub fn admits(&self, k: f64, e: f64, r: f64) -> bool {
        k >= self.k_floor && e >= self.e_floor && r <= self.r_ceiling
    }
}

/// Well-known corridor bands matching the canonical ecosafety grammar.
pub fn standard_corridor_bands() -> Vec<CorridorBand> {
    vec![
        CorridorBand {
            lane:       "RESEARCH".into(),
            k_floor:    0.80,
            e_floor:    0.70,
            r_ceiling:  0.30,
            vt_epsilon: 1e-4,
        },
        CorridorBand {
            lane:       "EXPPROD".into(),
            k_floor:    0.88,
            e_floor:    0.85,
            r_ceiling:  0.20,
            vt_epsilon: 1e-5,
        },
        CorridorBand {
            lane:       "PROD".into(),
            k_floor:    0.90,
            e_floor:    0.90,
            r_ceiling:  0.13,
            vt_epsilon: 1e-6,
        },
    ]
}

/// A single transition record in the fixture sequence.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FixtureStep {
    /// Step label for traceability, e.g. `"A→B"`.
    pub label: String,
    /// KER state before this transition.
    pub before: KerPoint,
    /// KER state after this transition.
    pub after: KerPoint,
}

/// A first-class publishable KER score shard.
///
/// This is the `qpudatashard` for the score lab.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerScoreShard {
    /// Lane for which this score applies.
    pub lane: String,
    /// Number of fixture steps replayed.
    pub steps_replayed: usize,
    /// Number of admissible steps (no invariant violations).
    pub steps_admissible: usize,
    /// Number of steps with invariant violations.
    pub steps_violated: usize,
    /// Final K at the end of the sequence.
    pub k_final: f64,
    /// Final E at the end of the sequence.
    pub e_final: f64,
    /// Final R at the end of the sequence.
    pub r_final: f64,
    /// Final Vt residual.
    pub vt_final: f64,
    /// Whether the final KER satisfies the corridor band.
    pub corridor_ok: bool,
    /// All violation messages collected during replay.
    pub violation_messages: Vec<String>,
    /// Owner DID.
    pub owner_did: String,
    /// Evidence hex (deterministic from owner_did + lane + steps).
    pub evidence_hex: String,
    /// Timestamp of scoring run (ISO-8601).
    pub scored_utc: String,
}

/// Replay `steps` through the invariant engine and produce a `KerScoreShard`.
///
/// `lane` selects the corridor band.  If the final KER violates the band,
/// `corridor_ok` is `false` and the caller should block the merge.
pub fn replay_and_score(
    steps: &[FixtureStep],
    lane: &str,
    owner_did: &str,
) -> Result<KerScoreShard, ScoreLabError> {
    if steps.is_empty() {
        return Err(ScoreLabError::EmptyFixture);
    }

    let bands = standard_corridor_bands();
    let band = bands
        .iter()
        .find(|b| b.lane == lane)
        .cloned()
        .unwrap_or(CorridorBand {
            lane:       lane.to_owned(),
            k_floor:    0.80,
            e_floor:    0.70,
            r_ceiling:  0.30,
            vt_epsilon: 1e-4,
        });

    let mut steps_admissible = 0usize;
    let mut steps_violated = 0usize;
    let mut violation_messages: Vec<String> = Vec::new();

    for step in steps {
        let result = check_all_invariants(&step.before, &step.after, band.vt_epsilon);
        if result.admissible {
            steps_admissible += 1;
        } else {
            steps_violated += 1;
            for v in &result.violations {
                violation_messages.push(format!("[{}] {}", step.label, v));
            }
        }
    }

    let last = steps.last().unwrap(); // safe: non-empty checked above
    let k_final = last.after.k;
    let e_final = last.after.e;
    let r_final = last.after.r;
    let vt_final = last.after.vt;

    let corridor_ok = band.admits(k_final, e_final, r_final) && steps_violated == 0;

    // Deterministic evidence hex: owner_did + lane + steps_replayed as a simple
    // concatenation fingerprint.  Not a cryptographic hash — a governance anchor.
    let evidence_hex = format!(
        "0xKERSCORE_{}_{}_{}_steps",
        owner_did.split('1').next().unwrap_or("eco"),
        lane,
        steps.len()
    );

    Ok(KerScoreShard {
        lane: lane.to_owned(),
        steps_replayed: steps.len(),
        steps_admissible,
        steps_violated,
        k_final,
        e_final,
        r_final,
        vt_final,
        corridor_ok,
        violation_messages,
        owner_did: owner_did.to_owned(),
        evidence_hex,
        scored_utc: Utc::now().format("%Y-%m-%dT%H:%M:%SZ").to_string(),
    })
}

/// Persist a `KerScoreShard` into the `ker_score_shard` table in `db_path`.
///
/// Schema is created if absent.
pub fn persist_score_shard(
    db_path: &str,
    shard: &KerScoreShard,
) -> Result<(), ScoreLabError> {
    let conn = rusqlite::Connection::open(db_path)?;
    conn.execute_batch(CREATE_SCORE_TABLE_SQL)?;

    conn.execute(
        r#"
        INSERT INTO ker_score_shard (
            lane, steps_replayed, steps_admissible, steps_violated,
            k_final, e_final, r_final, vt_final,
            corridor_ok, violation_messages_json,
            owner_did, evidence_hex, scored_utc
        ) VALUES (
            ?1, ?2, ?3, ?4,
            ?5, ?6, ?7, ?8,
            ?9, ?10,
            ?11, ?12, ?13
        )
        "#,
        rusqlite::params![
            shard.lane,
            shard.steps_replayed as i64,
            shard.steps_admissible as i64,
            shard.steps_violated as i64,
            shard.k_final,
            shard.e_final,
            shard.r_final,
            shard.vt_final,
            shard.corridor_ok as i64,
            serde_json::to_string(&shard.violation_messages)?,
            shard.owner_did,
            shard.evidence_hex,
            shard.scored_utc,
        ],
    )?;

    Ok(())
}

const CREATE_SCORE_TABLE_SQL: &str = r#"
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS ker_score_shard (
    score_id               INTEGER PRIMARY KEY AUTOINCREMENT,
    lane                   TEXT    NOT NULL,
    steps_replayed         INTEGER NOT NULL,
    steps_admissible       INTEGER NOT NULL,
    steps_violated         INTEGER NOT NULL,
    k_final                REAL    NOT NULL,
    e_final                REAL    NOT NULL,
    r_final                REAL    NOT NULL,
    vt_final               REAL    NOT NULL,
    corridor_ok            INTEGER NOT NULL DEFAULT 0 CHECK (corridor_ok IN (0,1)),
    violation_messages_json TEXT   NOT NULL,
    owner_did              TEXT    NOT NULL,
    evidence_hex           TEXT    NOT NULL,
    scored_utc             TEXT    NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_ker_score_shard_lane
    ON ker_score_shard (lane, scored_utc);
"#;

/// Build the canonical synthetic fixture for tests and CI.
///
/// Produces a sequence: A→B (admissible) followed by B→C (non-compensation violation).
pub fn canonical_synthetic_fixture() -> Vec<FixtureStep> {
    use kerdeployable::{synthetic_abc_fixture, PlaneRiskCoord};

    let (a, b, c) = synthetic_abc_fixture();

    vec![
        FixtureStep {
            label:  "A→B".into(),
            before: a.clone(),
            after:  b.clone(),
        },
        FixtureStep {
            label:  "B→C".into(),
            before: b,
            after:  c,
        },
    ]
}

#[cfg(test)]
mod tests {
    use super::*;

    const OWNER_DID: &str = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";

    #[test]
    fn replay_detects_violation_in_b_to_c() {
        let fixture = canonical_synthetic_fixture();
        let shard = replay_and_score(&fixture, "RESEARCH", OWNER_DID).unwrap();
        assert_eq!(shard.steps_replayed, 2);
        assert_eq!(shard.steps_admissible, 1, "A→B must be admissible");
        assert_eq!(shard.steps_violated, 1, "B→C must be violated");
        assert!(
            !shard.corridor_ok,
            "corridor_ok must be false when any step violated"
        );
    }

    #[test]
    fn replay_all_admissible_passes_corridor() {
        use kerdeployable::{synthetic_abc_fixture, PlaneRiskCoord};
        let (a, b, _) = synthetic_abc_fixture();
        let fixture = vec![FixtureStep {
            label:  "A→B".into(),
            before: a,
            after:  b,
        }];
        let shard = replay_and_score(&fixture, "RESEARCH", OWNER_DID).unwrap();
        assert_eq!(shard.steps_violated, 0);
        assert!(shard.corridor_ok);
    }

    #[test]
    fn persist_round_trip() {
        let fixture = canonical_synthetic_fixture();
        let shard = replay_and_score(&fixture, "RESEARCH", OWNER_DID).unwrap();

        let tmp = tempfile_path();
        persist_score_shard(&tmp, &shard).expect("persist failed");

        let conn = rusqlite::Connection::open(&tmp).unwrap();
        let count: i64 = conn
            .query_row("SELECT COUNT(*) FROM ker_score_shard", [], |r| r.get(0))
            .unwrap();
        assert_eq!(count, 1);
        let _ = std::fs::remove_file(&tmp);
    }

    fn tempfile_path() -> String {
        format!("/tmp/ker_score_test_{}.db", std::process::id())
    }
}
