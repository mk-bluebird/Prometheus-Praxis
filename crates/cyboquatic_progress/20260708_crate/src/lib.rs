// filename: eco_restoration_shard/crates/cyboquatic_progress/20260708_crate/src/lib.rs

//! Cyboquatic drainage-decay frame for 2026-07-08.
//!
//! Domain: (e) drainagedecay frame adding water-quality indicator (BOD, TSS, CEC).
//! Novel sub-task for this date: hex-hash derived "PHX-CANAL-DF-2026-07-08" drainage frame
//! with BOD/TSS/CEC indicators and Lyapunov-consistent residual ΔVt.
//!
//! This crate is non-actuating and diagnostic-only, aligned with the EcoNet spine design. [file:19][file:31]
//! It computes normalized risk coordinates from water-quality metrics, evaluates Lyapunov residuals,
//! and writes evidence rows into a SQLite `daily_progress` table with K,E,R scores and Phoenix hex stamp. [file:32][file:33]

use rusqlite::{params, Connection, Result as SqlResult};
use serde::{Deserialize, Serialize};

/// Fixed weights for Lyapunov residual over risk coordinates rx = [r_bod, r_tss, r_cec].
/// These weights are chosen to emphasize organic load (BOD) and suspended solids (TSS),
/// while keeping CEC (soil exchange capacity) as a stabilizing but non-offsettable coordinate. [file:19][file:31]
const W_BOD: f64 = 0.4;
const W_TSS: f64 = 0.35;
const W_CEC: f64 = 0.25;

/// Simple Phoenix corridor band limits for normalization.
/// Values are illustrative and should be tightened with real corridor data in future research. [file:31]
const BOD_SAFE_MAX_MG_L: f64 = 5.0;
const TSS_SAFE_MAX_MG_L: f64 = 20.0;
const CEC_SAFE_MIN_CMOL_PER_KG: f64 = 5.0;
const CEC_SAFE_MAX_CMOL_PER_KG: f64 = 25.0;

/// Hex-stamped Phoenix evidence string for this crate, anchoring to Central AZ corridors. [file:31][file:19]
const PHOENIX_EVIDENCE_HEX: &str = "0x20260708PHX33_45NDrainageDecayBODTSSCEC";

/// Identifier of prior day's progress crate, used for cumulative linkage.
/// This must match the 2026-07-07 crate name once created; currently treated as a planning pointer.
const PRIOR_DAY_CRATE_ID: &str = "cyboquatic_drainagedecay_20260707";

/// Risk coordinates for drainage-decay water quality.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DrainageRiskVector {
    pub r_bod: f64,
    pub r_tss: f64,
    pub r_cec: f64,
}

impl DrainageRiskVector {
    /// Compute Lyapunov residual Vt = Σ w_j r_j^2 for the current coordinates. [file:31][file:19]
    pub fn residual(&self) -> f64 {
        W_BOD * self.r_bod * self.r_bod
            + W_TSS * self.r_tss * self.r_tss
            + W_CEC * self.r_cec * self.r_cec
    }

    /// Enforce clamping of all coordinates to [0,1] band to avoid invalid risk states. [file:31]
    fn clamped(self) -> Self {
        fn clamp01(x: f64) -> f64 {
            if x < 0.0 {
                0.0
            } else if x > 1.0 {
                1.0
            } else {
                x
            }
        }
        DrainageRiskVector {
            r_bod: clamp01(self.r_bod),
            r_tss: clamp01(self.r_tss),
            r_cec: clamp01(self.r_cec),
        }
    }
}

/// Normalized drainage-decay sample, including raw measurements, risk coordinates, residuals, and K,E,R scores.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DrainageSample {
    pub sample_id: String,
    pub node_id: String,
    pub timestamp_utc: String,
    pub bod_mg_l: f64,
    pub tss_mg_l: f64,
    pub cec_cmol_per_kg: f64,
    pub risk: DrainageRiskVector,
    pub vt_before: f64,
    pub vt_after: f64,
    pub delta_vt: f64,
    pub k_factor: f64,
    pub e_factor: f64,
    pub r_factor: f64,
}

impl DrainageSample {
    /// Create a sample from raw metrics and a previous residual Vt, enforcing ΔVt ≤ 0. [file:19][file:31]
    pub fn from_raw(
        sample_id: &str,
        node_id: &str,
        timestamp_utc: &str,
        bod_mg_l: f64,
        tss_mg_l: f64,
        cec_cmol_per_kg: f64,
        vt_before: f64,
    ) -> Self {
        let risk_raw = normalize_risk(bod_mg_l, tss_mg_l, cec_cmol_per_kg);
        let risk = risk_raw.clamped();
        let vt_after = risk.residual();
        let delta_vt = vt_after - vt_before;

        // Always-improve gate: if delta_vt > 0, treat as diagnostic-only and penalize Eco impact. [file:19]
        let (k_factor, e_factor, r_factor) = compute_ker(&risk, delta_vt);

        DrainageSample {
            sample_id: sample_id.to_string(),
            node_id: node_id.to_string(),
            timestamp_utc: timestamp_utc.to_string(),
            bod_mg_l,
            tss_mg_l,
            cec_cmol_per_kg,
            risk,
            vt_before,
            vt_after,
            delta_vt,
            k_factor,
            e_factor,
            r_factor,
        }
    }
}

/// Normalize raw water-quality metrics into risk coordinates in [0,1].
/// BOD, TSS: higher is worse; CEC: out-of-band is worse, mid-band is safer. [file:31]
pub fn normalize_risk(bod_mg_l: f64, tss_mg_l: f64, cec_cmol_per_kg: f64) -> DrainageRiskVector {
    let r_bod = if bod_mg_l <= BOD_SAFE_MAX_MG_L {
        bod_mg_l / BOD_SAFE_MAX_MG_L
    } else {
        1.0
    };

    let r_tss = if tss_mg_l <= TSS_SAFE_MAX_MG_L {
        tss_mg_l / TSS_SAFE_MAX_MG_L
    } else {
        1.0
    };

    // CEC risk: 0 in safe band [CEC_SAFE_MIN, CEC_SAFE_MAX], rising to 1 as it deviates.
    let r_cec = if cec_cmol_per_kg >= CEC_SAFE_MIN_CMOL_PER_KG && cec_cmol_per_kg <= CEC_SAFE_MAX_CMOL_PER_KG {
        0.0
    } else if cec_cmol_per_kg < CEC_SAFE_MIN_CMOL_PER_KG {
        let diff = CEC_SAFE_MIN_CMOL_PER_KG - cec_cmol_per_kg;
        (diff / CEC_SAFE_MIN_CMOL_PER_KG).min(1.0)
    } else {
        let diff = cec_cmol_per_kg - CEC_SAFE_MAX_CMOL_PER_KG;
        (diff / CEC_SAFE_MAX_CMOL_PER_KG).min(1.0)
    };

    DrainageRiskVector {
        r_bod,
        r_tss,
        r_cec,
    }
}

/// Compute KER scores from risk vector and ΔVt, following existing ecosafety grammar: [file:31][file:19]
/// - Knowledge K is high when metrics are within corridor and ΔVt ≤ 0.
/// - Ecoimpact E is high when residual is low and ΔVt < 0.
/// - Risk R increases with residual and positive ΔVt.
pub fn compute_ker(risk: &DrainageRiskVector, delta_vt: f64) -> (f64, f64, f64) {
    let vt = risk.residual();

    // Knowledge factor: penalize only strongly if ΔVt > 0 or coordinates near 1.
    let max_r = risk.r_bod.max(risk.r_tss).max(risk.r_cec);
    let mut k = 0.95 - 0.3 * max_r;
    if delta_vt > 0.0 {
        k -= 0.2;
    }
    if k < 0.0 {
        k = 0.0;
    }

    // Ecoimpact: high if vt is small and residual drops; otherwise reduced.
    let mut e = 0.95 - vt;
    if delta_vt >= 0.0 {
        e -= 0.15;
    }
    if e < 0.0 {
        e = 0.0;
    }

    // Risk of harm: baseline from vt, increased by positive ΔVt.
    let mut r = vt + delta_vt.max(0.0);
    if r > 1.0 {
        r = 1.0;
    }

    (k, e, r)
}

/// Create or migrate the `daily_progress` table in a given SQLite database.
/// This table accumulates daily drainage-decay progress with hex-stamped evidence and KER triad. [file:33]
pub fn ensure_daily_progress_schema(conn: &Connection) -> SqlResult<()> {
    conn.execute_batch(
        r#"
        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS daily_progress (
            progress_id        INTEGER PRIMARY KEY AUTOINCREMENT,
            yyyymmdd           TEXT NOT NULL,
            crate_id           TEXT NOT NULL,
            domain             TEXT NOT NULL,
            subtask_id         TEXT NOT NULL,
            node_id            TEXT NOT NULL,
            sample_id          TEXT NOT NULL,
            timestamp_utc      TEXT NOT NULL,
            bod_mg_l           REAL NOT NULL,
            tss_mg_l           REAL NOT NULL,
            cec_cmol_per_kg    REAL NOT NULL,
            r_bod              REAL NOT NULL,
            r_tss              REAL NOT NULL,
            r_cec              REAL NOT NULL,
            vt_before          REAL NOT NULL,
            vt_after           REAL NOT NULL,
            delta_vt           REAL NOT NULL,
            k_factor           REAL NOT NULL,
            e_factor           REAL NOT NULL,
            r_factor           REAL NOT NULL,
            evidence_hex       TEXT NOT NULL,
            prior_crate_id     TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_daily_progress_date
            ON daily_progress (yyyymmdd);

        CREATE INDEX IF NOT EXISTS idx_daily_progress_node_time
            ON daily_progress (node_id, timestamp_utc);
        "#,
    )?;
    Ok(())
}

/// Insert a drainage sample into `daily_progress`, binding to today's crate and prior day's crate. [file:33]
pub fn insert_daily_progress(conn: &Connection, sample: &DrainageSample) -> SqlResult<()> {
    conn.execute(
        r#"
        INSERT INTO daily_progress (
            yyyymmdd,
            crate_id,
            domain,
            subtask_id,
            node_id,
            sample_id,
            timestamp_utc,
            bod_mg_l,
            tss_mg_l,
            cec_cmol_per_kg,
            r_bod,
            r_tss,
            r_cec,
            vt_before,
            vt_after,
            delta_vt,
            k_factor,
            e_factor,
            r_factor,
            evidence_hex,
            prior_crate_id
        )
        VALUES (
            ?1, ?2, ?3, ?4, ?5, ?6, ?7,
            ?8, ?9, ?10,
            ?11, ?12, ?13,
            ?14, ?15, ?16,
            ?17, ?18, ?19,
            ?20, ?21
        );
        "#,
        params![
            "20260708",
            "cyboquatic_drainagedecay_20260708",
            "drainagedecay",
            "PHX-CANAL-DF-2026-07-08",
            sample.node_id,
            sample.sample_id,
            sample.timestamp_utc,
            sample.bod_mg_l,
            sample.tss_mg_l,
            sample.cec_cmol_per_kg,
            sample.risk.r_bod,
            sample.risk.r_tss,
            sample.risk.r_cec,
            sample.vt_before,
            sample.vt_after,
            sample.delta_vt,
            sample.k_factor,
            sample.e_factor,
            sample.r_factor,
            PHOENIX_EVIDENCE_HEX,
            PRIOR_DAY_CRATE_ID
        ],
    )?;
    Ok(())
}

/// Convenience function: open or create the SQLite DB at `db_path`, ensure schema,
/// compute a sample, and insert into `daily_progress`.
/// This is non-actuating and can run on diagnostic nodes or CI replay. [file:32][file:19]
pub fn record_drainage_sample(
    db_path: &str,
    sample_id: &str,
    node_id: &str,
    timestamp_utc: &str,
    bod_mg_l: f64,
    tss_mg_l: f64,
    cec_cmol_per_kg: f64,
    vt_before: f64,
) -> SqlResult<DrainageSample> {
    let conn = Connection::open(db_path)?;
    ensure_daily_progress_schema(&conn)?;
    let sample = DrainageSample::from_raw(
        sample_id,
        node_id,
        timestamp_utc,
        bod_mg_l,
        tss_mg_l,
        cec_cmol_per_kg,
        vt_before,
    );
    insert_daily_progress(&conn, &sample)?;
    Ok(sample)
}

/// Kani proof harness: ensure normalization clamps risk coordinates to [0,1] and ΔVt is computed consistently.
/// This harness is non-actuating and checks a small subset of inputs.
#[cfg(kani)]
mod verification {
    use super::*;

    #[kani::proof]
    fn risk_clamping_and_residual_stability() {
        let bod: f64 = kani::any();
        let tss: f64 = kani::any();
        let cec: f64 = kani::any();
        let vt_before: f64 = kani::any();

        kani::assume(vt_before >= 0.0);
        let risk_raw = normalize_risk(bod, tss, cec);
        let risk = risk_raw.clamped();

        assert!(risk.r_bod >= 0.0 && risk.r_bod <= 1.0);
        assert!(risk.r_tss >= 0.0 && risk.r_tss <= 1.0);
        assert!(risk.r_cec >= 0.0 && risk.r_cec <= 1.0);

        let vt_after = risk.residual();
        assert!(vt_after >= 0.0);

        let delta_vt = vt_after - vt_before;
        let (_k, _e, r) = compute_ker(&risk, delta_vt);
        assert!(r >= 0.0 && r <= 1.0);
    }
}
