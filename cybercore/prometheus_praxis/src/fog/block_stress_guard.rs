// filepath: cybercore/prometheus_praxis/src/fog/block_stress_guard.rs

#![forbid(unsafe_code)]
#![warn(missing_docs)]

use serde::{Deserialize, Serialize};

use crate::lyapunov::block_adapter::{
    LyapunovBlockProjection,
    LyapunovCellProjection,
};

/// Advisory decision for a FOG block under combined Lyapunov + CBF constraints.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FogBlockStressDecision {
    /// True if all per-cell and per-block invariants hold.
    pub ok: bool,
    /// True if Lyapunov residual is within the admissible band.
    pub lyapunov_ok: bool,
    /// True if control barrier function (CBF) predicates hold.
    pub cbf_ok: bool,
    /// True if hydraulic and structural stress are within corridors.
    pub stress_ok: bool,
    /// Maximum observed V_t over the block.
    pub vt_max: f64,
    /// Maximum observed hydraulic stress index over the block.
    pub stress_max: f64,
    /// Human‑readable summary for logs and qpudatashards.
    pub reason: String,
}

/// Non‑actuating guard wiring Lyapunov block projections into a FOG‑safe decision.
#[derive(Debug, Default)]
pub struct FogBlockStressGuard {
    /// Lyapunov ceiling for this band and lane (e.g., Phoenix V_t band).
    pub vt_ceiling: f64,
    /// Maximum admissible hydraulic stress index before we consider the block unsafe.
    pub stress_ceiling: f64,
    /// Minimum acceptable control barrier function margin.
    pub cbf_margin_min: f64,
}

impl FogBlockStressGuard {
    /// Construct a guard with explicit corridors.
    pub fn new(vt_ceiling: f64, stress_ceiling: f64, cbf_margin_min: f64) -> Self {
        Self {
            vt_ceiling,
            stress_ceiling,
            cbf_margin_min,
        }
    }

    /// Evaluate a non‑actuating block decision from Lyapunov cell projections.
    ///
    /// This function does not touch IO or actuators; it purely maps risk coordinates
    /// into a boolean decision and a reason string.
    pub fn evaluate_block(
        &self,
        block: &LyapunovBlockProjection,
        cells: &[LyapunovCellProjection],
    ) -> FogBlockStressDecision {
        let mut vt_max = f64::NEG_INFINITY;
        let mut stress_max = f64::NEG_INFINITY;
        let mut lyapunov_ok = true;
        let mut cbf_ok = true;
        let mut stress_ok = true;

        for cell in cells {
            if let Some(vt) = cell.vt {
                if vt > vt_max {
                    vt_max = vt;
                }
                if vt > self.vt_ceiling {
                    lyapunov_ok = false;
                }
            }

            if let Some(stress) = cell.stress_index {
                if stress > stress_max {
                    stress_max = stress;
                }
                if stress > self.stress_ceiling {
                    stress_ok = false;
                }
            }

            if let Some(cbf_margin) = cell.cbf_margin {
                if cbf_margin < self.cbf_margin_min {
                    cbf_ok = false;
                }
            }
        }

        if !vt_max.is_finite() {
            vt_max = 0.0;
        }
        if !stress_max.is_finite() {
            stress_max = 0.0;
        }

        let ok = lyapunov_ok && cbf_ok && stress_ok;

        let reason = if ok {
            format!(
                "FOG block OK: Vt_max={:.4}<= {:.4}, stress_max={:.4}<= {:.4}, cbf_margin>= {:.4}",
                vt_max,
                self.vt_ceiling,
                stress_max,
                self.stress_ceiling,
                self.cbf_margin_min
            )
        } else {
            let mut parts = Vec::new();
            if !lyapunov_ok {
                parts.push(format!(
                    "Lyapunov violation: Vt_max={:.4} > {:.4}",
                    vt_max, self.vt_ceiling
                ));
            }
            if !stress_ok {
                parts.push(format!(
                    "Hydraulic/structural stress violation: stress_max={:.4} > {:.4}",
                    stress_max, self.stress_ceiling
                ));
            }
            if !cbf_ok {
                parts.push(format!(
                    "CBF margin violation: margin < {:.4}",
                    self.cbf_margin_min
                ));
            }
            if parts.is_empty() {
                parts.push("Unknown block stress anomaly".to_string());
            }
            format!("FOG block NOT OK: {}", parts.join("; "))
        };

        FogBlockStressDecision {
            ok,
            lyapunov_ok,
            cbf_ok,
            stress_ok,
            vt_max,
            stress_max,
            reason,
        }
    }
}
