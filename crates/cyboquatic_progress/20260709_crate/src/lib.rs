// filename: eco_restoration_shard/crates/cyboquatic_progress/20260709_crate/src/lib.rs

#![forbid(unsafe_code)]
#![deny(missing_docs)]
#![deny(clippy::unwrap_used)]
#![deny(clippy::expect_used)]
#![deny(clippy::panic)]

use rusqlite::{params, Connection, Result as SqlResult};
use serde::{Deserialize, Serialize};

/// Domain (d): cyboquatic workload with energy requirement (J) and Lyapunov residual ΔVt.
/// Novel sub-task for 2026-07-09 is encoded as PHX-CANAL-WL-2026-07-09 and binds per-node workloads
/// to energyreq_j, an energy-tailwind indicator, and residual change ΔVt, all non-actuating and diagnostic-only.

/// Fixed Lyapunov weights for workload residual over three risk coordinates:
/// r_energy (shortfall/tailwind), r_hydraulic (hydraulic risk proxy), r_uncertainty (sensor/model trust).
const W_ENERGY: f64 = 0.8;
const W_HYDRAULIC: f64 = 1.0;
const W_UNCERTAINTY: f64 = 0.6;

/// Simple Phoenix corridor limits for energy and hydraulics.
/// Values are illustrative and should be refined with real Phoenix canal workload data in later research.
const ENERGY_TAILWIND_SAFE_RATIO: f64 = 1.2; // Esurplus / Ereq >= 1.2 is strong tailwind
const ENERGY_MIN_RATIO: f64 = 0.0;
const ENERGY_MAX_RATIO: f64 = 2.5;

const HYDRAULIC_SAFE_MAX: f64 = 0.3; // normalized hydraulic risk <= 0.3 considered safe
const HYDRAULIC_HARD_MAX: f64 = 1.0;

/// Evidence hex stamp for Phoenix workload corridor on this date.
const PHOENIX_EVIDENCE_HEX: &str = "0x20260709PHX3345NWorkloadEnergyDeltaVt";

/// Identifier of prior day's progress crate to maintain cumulative chain.
const PRIOR_DAY_CRATE_ID: &str = "cyboquaticdrainagedecay20260708";

/// Risk coordinates for cyboquatic workload.
/// All fields are expected in the closed interval [0.0, 1.0] after normalization and clamping.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkloadRiskVector {
    /// Energy risk coordinate: 0 safe tailwind, 1 severe energy shortfall.
    pub r_energy: f64,
    /// Hydraulic risk coordinate: 0 hydraulically safe, 1 high surcharge / unstable flow.
    pub r_hydraulic: f64,
    /// Uncertainty risk coordinate: 0 fully trusted telemetry/model, 1 unusable.
    pub r_uncertainty: f64,
}

impl WorkloadRiskVector {
    /// Compute Lyapunov residual Vt = Σ w_j * r_j^2 for the current risk coordinates.
    pub fn residual(&self) -> f64 {
        W_ENERGY * self.r_energy * self.r_energy
            + W_HYDRAULIC * self.r_hydraulic * self.r_hydraulic
            + W_UNCERTAINTY * self.r_uncertainty * self.r_uncertainty
    }

    /// Clamp all risk coordinates into [0.0, 1.0] to preserve invariant domain.
    fn clamped(&self) -> Self {
        fn clamp01(x: f64) -> f64 {
            if x < 0.0 {
                0.0
            } else if x > 1.0 {
                1.0
            } else {
                x
            }
        }

        WorkloadRiskVector {
            r_energy: clamp01(self.r_energy),
            r_hydraulic: clamp01(self.r_hydraulic),
            r_uncertainty: clamp01(self.r_uncertainty),
        }
    }
}

/// Cyboquatic workload diagnostic sample.
/// Contains raw energetics, normalized risk, Lyapunov residuals, and K,E,R scores.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkloadSample {
    /// Unique sample identifier for this workload measurement.
    pub sample_id: String,
    /// Node identifier e.g., PHX-CANAL-NODE-WL-01.
    pub node_id: String,
    /// Timestamp in UTC ISO-8601 (string to avoid time-zone coupling).
    pub timestamp_utc: String,
    /// Required energy for workload, in Joules.
    pub energy_req_j: f64,
    /// Available surplus energy at node, in Joules.
    pub energy_surplus_j: f64,
    /// Hydraulic risk proxy in [0,1] (e.g., surcharge index, velocity/HLR kernel output).
    pub hydraulic_risk: f64,
    /// Uncertainty risk in [0,1] from sensor health and model trust.
    pub uncertainty_risk: f64,
    /// Normalized workload risk coordinates.
    pub risk: WorkloadRiskVector,
    /// Previous Lyapunov residual Vt_before for this node/workload window.
    pub vt_before: f64,
    /// New Lyapunov residual Vt_after after applying this workload.
    pub vt_after: f64,
    /// Residual change ΔVt = Vt_after - Vt_before.
    pub delta_vt: f64,
    /// Knowledge factor K (0..1) for this workload evidence kernel.
    pub k_factor: f64,
    /// Eco-impact factor E (0..1) derived from residual behaviour and energy tailwind.
    pub e_factor: f64,
    /// Risk-of-harm factor R (0..1) from residual and risk coordinates.
    pub r_factor: f64,
}

impl WorkloadSample {
    /// Construct a workload sample from raw energetics and risk proxies, enforcing residual and KER invariants.
    ///
    /// This function is non-actuating and suitable for diagnostic/CI use.
    pub fn from_raw(
        sample_id: &str,
        node_id: &str,
        timestamp_utc: &str,
        energy_req_j: f64,
        energy_surplus_j: f64,
        hydraulic_risk: f64,
        uncertainty_risk: f64,
        vt_before: f64,
    ) -> Self {
        let risk_raw = normalize_risk(energy_req_j, energy_surplus_j, hydraulic_risk, uncertainty_risk);
        let risk = risk_raw.clamped();
        let vt_after = risk.residual();
        let delta_vt = vt_after - vt_before;

        let (k_factor, e_factor, r_factor) = compute_ker(&risk, delta_vt);

        WorkloadSample {
            sample_id: sample_id.to_string(),
            node_id: node_id.to_string(),
            timestamp_utc: timestamp_utc.to_string(),
            energy_req_j,
            energy_surplus_j,
            hydraulic_risk,
            uncertainty_risk,
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

/// Normalize raw energetics and risk proxies into risk coordinates r_energy, r_hydraulic, r_uncertainty in [0,1].
///
/// Energy risk: shortfall relative to requirement.
/// Tailwind ratio R_t = Esurplus / Ereq; high ratio is safe tailwind, low ratio is high risk.
/// Hydraulic and uncertainty risk are passed through but clamped, assuming upstream kernels already map them to [0,1].
pub fn normalize_risk(
    energy_req_j: f64,
    energy_surplus_j: f64,
    hydraulic_risk: f64,
    uncertainty_risk: f64,
) -> WorkloadRiskVector {
    let ratio = if energy_req_j <= 0.0 {
        ENERGY_MAX_RATIO
    } else {
        energy_surplus_j / energy_req_j
    };

    // Map ratio to risk: tailwind (ratio >= SAFE) -> low risk; severe shortfall (ratio ~ 0) -> high risk.
    let r_energy_raw = if ratio >= ENERGY_TAILWIND_SAFE_RATIO {
        0.0
    } else if ratio <= ENERGY_MIN_RATIO {
        1.0
    } else {
        let bounded_ratio = if ratio > ENERGY_MAX_RATIO {
            ENERGY_MAX_RATIO
        } else {
            ratio
        };
        // Linear mapping between 0 and SAFE ratio: risk decreases as ratio increases.
        let span = ENERGY_TAILWIND_SAFE_RATIO - ENERGY_MIN_RATIO;
        let rel = (bounded_ratio - ENERGY_MIN_RATIO) / span;
        1.0 - rel
    };

    WorkloadRiskVector {
        r_energy: r_energy_raw,
        r_hydraulic: hydraulic_risk,
        r_uncertainty: uncertainty_risk,
    }
}

/// Compute K,E,R scores from risk vector and ΔVt.
/// - K high when residual does not increase and energy risk is low.
/// - E high when residual is low and ΔVt <= 0 (Lyapunov-safe workload).
/// - R increases with residual and positive ΔVt, clipped to [0,1].
pub fn compute_ker(risk: &WorkloadRiskVector, delta_vt: f64) -> (f64, f64, f64) {
    let vt = risk.residual();

    // Knowledge factor: penalize high energy risk and positive ΔVt.
    let max_r = risk.r_energy.max(risk.r_hydraulic).max(risk.r_uncertainty);
    let mut k = 0.95 - 0.4 * max_r;
    if delta_vt > 0.0 {
        k -= 0.25;
    }
    if k < 0.0 {
        k = 0.0;
    }
    if k > 1.0 {
        k = 1.0;
    }

    // Eco-impact factor: high when vt is small and workload improves or maintains residual.
    let mut e = 0.95 - vt;
    if delta_vt > 0.0 {
        e -= 0.3;
    }
    if e < 0.0 {
        e = 0.0;
    }
    if e > 1.0 {
        e = 1.0;
    }

    // Risk-of-harm factor: baseline from vt, increased by positive ΔVt.
    let mut r = vt + delta_vt.max(0.0);
    if r > 1.0 {
        r = 1.0;
    }
    if r < 0.0 {
        r = 0.0;
    }

    (k, e, r)
}

/// Ensure the daily_progress table exists in the given SQLite database.
/// This table accumulates daily workload progress with Phoenix evidence and KER triad.
pub fn ensure_daily_progress_schema(conn: &Connection) -> SqlResult<()> {
    conn.execute_batch(
        r#"
        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS daily_progress (
            progress_id      INTEGER PRIMARY KEY AUTOINCREMENT,
            yyyymmdd         TEXT NOT NULL,
            crate_id         TEXT NOT NULL,
            domain           TEXT NOT NULL,
            subtask_id       TEXT NOT NULL,
            node_id          TEXT NOT NULL,
            sample_id        TEXT NOT NULL,
            timestamp_utc    TEXT NOT NULL,
            energy_req_j     REAL NOT NULL,
            energy_surplus_j REAL NOT NULL,
            hydraulic_risk   REAL NOT NULL,
            uncertainty_risk REAL NOT NULL,
            r_energy         REAL NOT NULL,
            r_hydraulic      REAL NOT NULL,
            r_uncertainty    REAL NOT NULL,
            vt_before        REAL NOT NULL,
            vt_after         REAL NOT NULL,
            delta_vt         REAL NOT NULL,
            k_factor         REAL NOT NULL,
            e_factor         REAL NOT NULL,
            r_factor         REAL NOT NULL,
            evidence_hex     TEXT NOT NULL,
            prior_crate_id   TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_daily_progress_date
            ON daily_progress (yyyymmdd);

        CREATE INDEX IF NOT EXISTS idx_daily_progress_node_time
            ON daily_progress (node_id, timestamp_utc);
        "#,
    )?;
    Ok(())
}

/// Insert a workload sample into the daily_progress table, bound to today's crate and prior day's crate.
pub fn insert_daily_progress(conn: &Connection, sample: &WorkloadSample) -> SqlResult<usize> {
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
            energy_req_j,
            energy_surplus_j,
            hydraulic_risk,
            uncertainty_risk,
            r_energy,
            r_hydraulic,
            r_uncertainty,
            vt_before,
            vt_after,
            delta_vt,
            k_factor,
            e_factor,
            r_factor,
            evidence_hex,
            prior_crate_id
        ) VALUES (
            ?1, ?2, ?3, ?4, ?5,
            ?6, ?7, ?8, ?9, ?10,
            ?11, ?12, ?13, ?14, ?15,
            ?16, ?17, ?18, ?19, ?20,
            ?21, ?22
        );
        "#,
        params![
            "20260709",
            "cyboquatic_workload20260709",
            "workload_energy_delta_vt",
            "PHX-CANAL-WL-2026-07-09",
            sample.node_id,
            sample.sample_id,
            sample.timestamp_utc,
            sample.energy_req_j,
            sample.energy_surplus_j,
            sample.hydraulic_risk,
            sample.uncertainty_risk,
            sample.risk.r_energy,
            sample.risk.r_hydraulic,
            sample.risk.r_uncertainty,
            sample.vt_before,
            sample.vt_after,
            sample.delta_vt,
            sample.k_factor,
            sample.e_factor,
            sample.r_factor,
            PHOENIX_EVIDENCE_HEX,
            PRIOR_DAY_CRATE_ID,
        ],
    )
}

/// Convenience function: open or create SQLite DB, ensure schema, compute sample, and insert.
/// Returns the fully populated WorkloadSample for further diagnostics.
pub fn record_workload_sample(
    db_path: &str,
    sample_id: &str,
    node_id: &str,
    timestamp_utc: &str,
    energy_req_j: f64,
    energy_surplus_j: f64,
    hydraulic_risk: f64,
    uncertainty_risk: f64,
    vt_before: f64,
) -> SqlResult<WorkloadSample> {
    let conn = Connection::open(db_path)?;
    ensure_daily_progress_schema(&conn)?;
    let sample = WorkloadSample::from_raw(
        sample_id,
        node_id,
        timestamp_utc,
        energy_req_j,
        energy_surplus_j,
        hydraulic_risk,
        uncertainty_risk,
        vt_before,
    );
    insert_daily_progress(&conn, &sample)?;
    Ok(sample)
}

#[cfg(kani)]
mod verification {
    use super::*;
    use kani::prelude::*;

    /// Kani harness: check that normalization clamps risk coordinates to [0,1],
    /// residuals are non-negative, and R stays in [0,1] for admissible inputs.
    #[kani::proof]
    fn workload_risk_clamping_and_residual_safety() {
        let energy_req_j: f64 = kani::any();
        let energy_surplus_j: f64 = kani::any();
        let hydraulic_risk: f64 = kani::any();
        let uncertainty_risk: f64 = kani::any();
        let vt_before: f64 = kani::any();

        kani::assume(energy_req_j >= 0.0);
        kani::assume(energy_surplus_j >= 0.0);
        kani::assume(hydraulic_risk >= 0.0 && hydraulic_risk <= 1.0);
        kani::assume(uncertainty_risk >= 0.0 && uncertainty_risk <= 1.0);
        kani::assume(vt_before >= 0.0);

        let risk_raw = normalize_risk(energy_req_j, energy_surplus_j, hydraulic_risk, uncertainty_risk);
        let risk = risk_raw.clamped();

        assert!(risk.r_energy >= 0.0 && risk.r_energy <= 1.0);
        assert!(risk.r_hydraulic >= 0.0 && risk.r_hydraulic <= 1.0);
        assert!(risk.r_uncertainty >= 0.0 && risk.r_uncertainty <= 1.0);

        let vt_after = risk.residual();
        assert!(vt_after >= 0.0);

        let delta_vt = vt_after - vt_before;
        let (k, e, r) = compute_ker(&risk, delta_vt);

        assert!(k >= 0.0 && k <= 1.0);
        assert!(e >= 0.0 && e <= 1.0);
        assert!(r >= 0.0 && r <= 1.0);
    }
}
