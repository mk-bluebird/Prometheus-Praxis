// filename: .github/workflows/ci-response-rcalib-rsigma.rs
// Purpose: CI invariant checks for rcalib / rsigma thresholds on eco_response_shard outputs.

use rusqlite::{params, Connection};
use std::path::Path;

/// Hard thresholds derived from existing ecosafety grammar:
/// - rcalib in [0, 1], rsigma in [0, 1]
/// - Any shard with rcalib > 0.40 is blocked from PROD lane (BlockedByCalib)
/// - Any shard with rsigma > 0.50 is blocked from PROD lane (BlockedByRisk)
/// - vt_after must be >= vt_before (non-negative incremental risk)
const RCALIB_HARD_MAX: f64 = 0.40;
const RSIGMA_HARD_MAX: f64 = 0.50;

/// CI entry point: run as part of eco_response_shard CI to validate invariants.
pub fn run_ci_invariants(db_path: &str) -> rusqlite::Result<()> {
    if !Path::new(db_path).exists() {
        eprintln!("eco_response_shard DB not found at {}", db_path);
        std::process::exit(1);
    }

    let conn = Connection::open(db_path)?;

    // 1. Check basic bounds for rcalib, rsigma.
    let mut stmt_bounds = conn.prepare(
        r#"
        SELECT response_id, rcalib, rsigma
        FROM response_calib_sigma
        WHERE rcalib < 0.0
           OR rcalib > 1.0
           OR rsigma < 0.0
           OR rsigma > 1.0;
        "#,
    )?;

    let mut bad_bounds = Vec::new();
    let bounds_rows = stmt_bounds.query_map([], |row| {
        Ok((
            row.get::<_, i64>(0)?,
            row.get::<_, f64>(1)?,
            row.get::<_, f64>(2)?,
        ))
    })?;

    for r in bounds_rows {
        bad_bounds.push(r?);
    }

    if !bad_bounds.is_empty() {
        eprintln!("CI invariant failed: rcalib/rsigma out of [0,1] for responses: {:?}", bad_bounds);
        std::process::exit(1);
    }

    // 2. Enforce PROD lane gating on rcalib / rsigma.
    let mut stmt_gate = conn.prepare(
        r#"
        SELECT
            c.response_id,
            r.lane,
            c.rcalib,
            c.rsigma
        FROM response_calib_sigma c
        JOIN response_shard r ON r.response_id = c.response_id
        WHERE r.lane = 'PROD'
          AND (c.rcalib > ?1 OR c.rsigma > ?2);
        "#,
    )?;

    let mut bad_prod = Vec::new();
    let gate_rows = stmt_gate.query_map(params![RCALIB_HARD_MAX, RSIGMA_HARD_MAX], |row| {
        Ok((
            row.get::<_, i64>(0)?,
            row.get::<_, String>(1)?,
            row.get::<_, f64>(2)?,
            row.get::<_, f64>(3)?,
        ))
    })?;

    for r in gate_rows {
        bad_prod.push(r?);
    }

    if !bad_prod.is_empty() {
        eprintln!(
            "CI invariant failed: PROD responses exceed rcalib/rsigma hard thresholds: {:?}",
            bad_prod
        );
        std::process::exit(1);
    }

    // 3. Enforce vt_after >= vt_before (non-negative incremental risk from rcalib/rsigma planes).
    let mut stmt_vt = conn.prepare(
        r#"
        SELECT response_id, vt_before, vt_after
        FROM response_calib_sigma
        WHERE vt_after + 1e-12 < vt_before;
        "#,
    )?;

    let mut bad_vt = Vec::new();
    let vt_rows = stmt_vt.query_map([], |row| {
        Ok((
            row.get::<_, i64>(0)?,
            row.get::<_, f64>(1)?,
            row.get::<_, f64>(2)?,
        ))
    })?;

    for r in vt_rows {
        bad_vt.push(r?);
    }

    if !bad_vt.is_empty() {
        eprintln!(
            "CI invariant failed: vt_after < vt_before for responses (rcalib/rsigma must not reduce residual): {:?}",
            bad_vt
        );
        std::process::exit(1);
    }

    // 4. Enforce deploy_decision consistency.
    let mut stmt_decision = conn.prepare(
        r#"
        SELECT response_id, rcalib, rsigma, deploy_decision, lane
        FROM response_calib_sigma
        WHERE
            (rcalib > ?1 AND deploy_decision != 'BlockedByCalib')
         OR (rsigma > ?2 AND deploy_decision NOT IN ('BlockedByRisk', 'BlockedByKER'))
         OR ((rcalib <= ?1 AND rsigma <= ?2) AND deploy_decision = 'BlockedByCalib');
        "#,
    )?;

    let mut bad_decisions = Vec::new();
    let dec_rows = stmt_decision.query_map(params![RCALIB_HARD_MAX, RSIGMA_HARD_MAX], |row| {
        Ok((
            row.get::<_, i64>(0)?,
            row.get::<_, f64>(1)?,
            row.get::<_, f64>(2)?,
            row.get::<_, String>(3)?,
            row.get::<_, String>(4)?,
        ))
    })?;

    for r in dec_rows {
        bad_decisions.push(r?);
    }

    if !bad_decisions.is_empty() {
        eprintln!(
            "CI invariant failed: deploy_decision inconsistent with rcalib/rsigma thresholds: {:?}",
            bad_decisions
        );
        std::process::exit(1);
    }

    Ok(())
}
