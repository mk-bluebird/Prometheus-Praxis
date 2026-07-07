// eco_restoration_shard/crates/cyboquatic_progress/20260705_crate/src/lib.rs
//! Daily cyboquatic_progress crate for 2026-07-05.
//! Domain (d): cyboquatic workload with energyreqJ and ΔVt.
//!
//! Sub-task: Phoenix canal biodeg workload shard for a distinct
//! biodegradable liner molecule ("PhxLin-20260705") with energy
//! requirement and Lyapunov residual gate on ΔVt.
//!
//! This crate is non-actuating and intended for diagnostics only.

use rusqlite::{params, Connection, OptionalExtension};
use serde::{Deserialize, Serialize};
use std::time::{SystemTime, UNIX_EPOCH};

/// KER triad for scoring knowledge, eco-impact, and risk-of-harm.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct KerScore {
    pub k: f64,
    pub e: f64,
    pub r: f64,
}

/// Cyboquatic workload descriptor for biodegradable machinery nodes.
/// This is purely a diagnostic workload; it must never drive actuators.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CyboquaticWorkload {
    /// Unique workload identifier (e.g., hash of date + corridor shard).
    pub workload_id: String,
    /// Phoenix corridor tag (e.g., canal segment).
    pub corridor_tag: String,
    /// Biodegradable liner molecule label for this day.
    pub molecule_label: String,
    /// Energy required in joules for one workload cycle.
    pub energy_req_j: f64,
    /// Local Lyapunov residual before workload.
    pub vt_before: f64,
    /// Local Lyapunov residual after workload.
    pub vt_after: f64,
    /// KER scores for this workload instance.
    pub ker: KerScore,
}

/// Daily progress row anchored to Phoenix and Bostrom DID.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DailyProgressRow {
    pub date_yyyymmdd: String,
    pub hex_evidence: String,
    pub phoenix_location: String,
    pub domain: String,
    pub sub_task: String,
    pub ker_k: f64,
    pub ker_e: f64,
    pub ker_r: f64,
    pub prev_pointer: Option<String>,
    pub workload_id: String,
}

/// Compute a simple integer hash of a YYYYMMDD date string to drive
/// sub-task selection without external randomness.
pub fn date_hash_32(date_yyyymmdd: &str) -> u32 {
    let mut h: u32 = 0x811c_9dc5;
    for b in date_yyyymmdd.bytes() {
        h ^= b as u32;
        h = h.wrapping_mul(0x0100_0193);
    }
    h
}

/// Derive a daily biodegradable liner molecule label from the date hash.
/// This ensures a distinct label per day ("PhxLin-<hex_hash>").
pub fn derive_molecule_label(date_yyyymmdd: &str) -> String {
    let h = date_hash_32(date_yyyymmdd);
    format!("PhxLin-{:08x}", h)
}

/// Compute ΔVt and enforce non-increasing residual invariant.
/// Returns Ok(ΔVt) if Vt_after <= Vt_before, otherwise Err.
pub fn delta_vt_guard(vt_before: f64, vt_after: f64) -> Result<f64, String> {
    let delta = vt_after - vt_before;
    if vt_after <= vt_before + 1.0e-9 {
        Ok(delta)
    } else {
        Err(format!(
            "Lyapunov violation: vt_after ({:.6}) > vt_before ({:.6})",
            vt_after, vt_before
        ))
    }
}

/// Simple energy corridor: clamp energy requirement into a normalized
/// risk coordinate r_energy in [0,1] using safe, gold, and hard bands.
/// Values above hard band saturate at 1.
pub fn normalize_energy_req(energy_req_j: f64) -> f64 {
    let safe_max = 50.0_f64;
    let hard_max = 500.0_f64;
    if energy_req_j <= safe_max {
        0.0
    } else if energy_req_j >= hard_max {
        1.0
    } else {
        let span = hard_max - safe_max;
        ((energy_req_j - safe_max) / span).min(1.0).max(0.0)
    }
}

/// Compute a KER score for a biodegradable cyboquatic workload.
/// - K: knowledge factor (fixed high for grammar-consistent shard).
/// - E: eco-impact (higher when r_energy is low and ΔVt <= 0).
/// - R: risk-of-harm (higher when r_energy is high or ΔVt > 0).
pub fn compute_ker(energy_req_j: f64, delta_vt: f64) -> KerScore {
    let r_energy = normalize_energy_req(energy_req_j);
    let k = 0.93_f64;

    let eco_base = 0.90_f64;
    let eco_penalty_energy = 0.20_f64 * r_energy;
    let eco_penalty_delta = if delta_vt <= 0.0 { 0.0 } else { 0.25_f64 };
    let e = (eco_base - eco_penalty_energy - eco_penalty_delta).max(0.0);

    let risk_base = 0.12_f64;
    let risk_energy = 0.30_f64 * r_energy;
    let risk_delta = if delta_vt <= 0.0 { 0.0 } else { 0.20_f64 };
    let r = (risk_base + risk_energy + risk_delta).min(1.0);

    KerScore { k, e, r }
}

/// Build a CyboquaticWorkload for the given date and Phoenix corridor.
/// vt_after is set to vt_before + dv, but must respect Lyapunov guard.
pub fn build_daily_workload(
    date_yyyymmdd: &str,
    corridor_tag: &str,
    vt_before: f64,
    vt_target_delta: f64,
) -> Result<CyboquaticWorkload, String> {
    let molecule_label = derive_molecule_label(date_yyyymmdd);

    // Deterministic energy requirement derived from date hash.
    let base_hash = date_hash_32(date_yyyymmdd) as f64;
    let energy_req_j = 25.0_f64 + (base_hash % 200) as f64; // 25–225 J

    let vt_after = vt_before + vt_target_delta;
    let delta_vt = delta_vt_guard(vt_before, vt_after)?;

    let ker = compute_ker(energy_req_j, delta_vt);

    Ok(CyboquaticWorkload {
        workload_id: format!("cwk-{}-{}", date_yyyymmdd, molecule_label),
        corridor_tag: corridor_tag.to_string(),
        molecule_label,
        energy_req_j,
        vt_before,
        vt_after,
        ker,
    })
}

/// Initialize (or open) the daily progress SQLite database.
/// If missing, create schema with daily_progress table.
pub fn init_daily_db(path: &str) -> rusqlite::Result<Connection> {
    let conn = Connection::open(path)?;
    conn.execute_batch(
        r#"
        PRAGMA journal_mode=WAL;
        PRAGMA foreign_keys=ON;

        CREATE TABLE IF NOT EXISTS daily_progress (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            date_yyyymmdd TEXT    NOT NULL,
            hex_evidence  TEXT    NOT NULL,
            phoenix_location TEXT NOT NULL,
            domain        TEXT    NOT NULL,
            sub_task      TEXT    NOT NULL,
            ker_k         REAL    NOT NULL,
            ker_e         REAL    NOT NULL,
            ker_r         REAL    NOT NULL,
            prev_pointer  TEXT,
            workload_id   TEXT    NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_daily_progress_date
            ON daily_progress (date_yyyymmdd);

        CREATE INDEX IF NOT EXISTS idx_daily_progress_workload
            ON daily_progress (workload_id);
        "#,
    )?;
    Ok(conn)
}

/// Fetch the most recent daily progress row, if any, to chain pointers.
pub fn fetch_latest_progress(conn: &Connection) -> rusqlite::Result<Option<DailyProgressRow>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            date_yyyymmdd,
            hex_evidence,
            phoenix_location,
            domain,
            sub_task,
            ker_k,
            ker_e,
            ker_r,
            prev_pointer,
            workload_id
        FROM daily_progress
        ORDER BY id DESC
        LIMIT 1;
        "#,
    )?;
    stmt.query_row([], |row| {
        Ok(DailyProgressRow {
            date_yyyymmdd: row.get(0)?,
            hex_evidence: row.get(1)?,
            phoenix_location: row.get(2)?,
            domain: row.get(3)?,
            sub_task: row.get(4)?,
            ker_k: row.get(5)?,
            ker_e: row.get(6)?,
            ker_r: row.get(7)?,
            prev_pointer: row.get(8)?,
            workload_id: row.get(9)?,
        })
    })
    .optional()
}

/// Insert today's progress row, chaining to previous workload_id if present.
pub fn insert_daily_progress(
    conn: &Connection,
    date_yyyymmdd: &str,
    workload: &CyboquaticWorkload,
) -> rusqlite::Result<DailyProgressRow> {
    let latest = fetch_latest_progress(conn)?;

    let hex_evidence = derive_hex_evidence(date_yyyymmdd, &workload.workload_id);
    let phoenix_location = "Phoenix-AZ-Canal-33.45N-112.07W".to_string();
    let domain = "cyboquatic_workload_energyreqJ_ΔVt".to_string();
    let sub_task = "Phx canal biodegradable liner workload shard".to_string();

    conn.execute(
        r#"
        INSERT INTO daily_progress (
            date_yyyymmdd,
            hex_evidence,
            phoenix_location,
            domain,
            sub_task,
            ker_k,
            ker_e,
            ker_r,
            prev_pointer,
            workload_id
        )
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10);
        "#,
        params![
            date_yyyymmdd,
            hex_evidence,
            phoenix_location,
            domain,
            sub_task,
            workload.ker.k,
            workload.ker.e,
            workload.ker.r,
            latest.as_ref().map(|r| r.workload_id.clone()),
            workload.workload_id,
        ],
    )?;

    Ok(DailyProgressRow {
        date_yyyymmdd: date_yyyymmdd.to_string(),
        hex_evidence,
        phoenix_location,
        domain,
        sub_task,
        ker_k: workload.ker.k,
        ker_e: workload.ker.e,
        ker_r: workload.ker.r,
        prev_pointer: latest.map(|r| r.workload_id),
        workload_id: workload.workload_id.clone(),
    })
}

/// Derive a deterministic hex evidence string from date and workload_id.
/// This is a simple, non-cryptographic stamp for chain-of-custody.
pub fn derive_hex_evidence(date_yyyymmdd: &str, workload_id: &str) -> String {
    let mut h: u64 = 0xcbf2_9ce4_8422_2325;
    for b in date_yyyymmdd.bytes().chain(workload_id.bytes()) {
        h ^= b as u64;
        h = h.wrapping_mul(0x100_0000_1b3);
    }
    format!("0x{:016x}", h)
}

/// Build and persist today's Phoenix cyboquatic workload shard.
/// Returns the stored DailyProgressRow for external tooling and KER audits.
pub fn run_today_progress(db_path: &str, date_yyyymmdd: &str) -> Result<DailyProgressRow, String> {
    let conn = init_daily_db(db_path).map_err(|e| e.to_string())?;

    // Non-actuating diagnostic workload for Phoenix canal biodegradable liner.
    let vt_before = 0.45_f64;
    let vt_target_delta = -0.02_f64;

    let workload = build_daily_workload(date_yyyymmdd, "Phoenix-Canal-Biodeg-Node-01", vt_before, vt_target_delta)?;
    let row = insert_daily_progress(&conn, date_yyyymmdd, &workload).map_err(|e| e.to_string())?;
    Ok(row)
}

/// Utility for ad-hoc runs: derive today's date in YYYYMMDD from UNIX time.
/// This function is non-critical and used only for local testing.
pub fn current_date_yyyymmdd() -> String {
    // This is a simple UTC-based approximation; production callers
    // should pass an explicit date string for corridor-safe runs.
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs();

    // Approximate days since epoch.
    let days = now / 86_400;
    // Unix epoch day number for 1970-01-01 is 0; we map to a rough date.
    // For precise Phoenix-aligned dates, external schedulers should call
    // run_today_progress with an explicit YYYYMMDD.
    let year = 1970 + (days / 365) as i32;
    let day_of_year = (days % 365) as i32;
    let month = 1 + (day_of_year / 30);
    let day = 1 + (day_of_year % 30);
    format!("{:04}{:02}{:02}", year, month, day)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn molecule_label_is_stable_and_unique_per_date() {
        let d1 = "20260705";
        let d2 = "20260706";
        let m1 = derive_molecule_label(d1);
        let m2 = derive_molecule_label(d2);
        assert_ne!(m1, m2);
        assert!(m1.starts_with("PhxLin-"));
        assert!(m2.starts_with("PhxLin-"));
    }

    #[test]
    fn delta_vt_guard_accepts_non_increasing() {
        let r = delta_vt_guard(0.5, 0.48).unwrap();
        assert!(r <= 0.0);
    }

    #[test]
    fn delta_vt_guard_rejects_increase() {
        let err = delta_vt_guard(0.5, 0.52).unwrap_err();
        assert!(err.contains("Lyapunov violation"));
    }

    #[test]
    fn ker_scores_increase_r_with_energy_and_delta() {
        let low = compute_ker(40.0, -0.01);
        let high = compute_ker(400.0, 0.01);
        assert!(low.e > high.e);
        assert!(low.r < high.r);
    }
}
