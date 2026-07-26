// filename: prometheus-praxis/spine/src/eco_credit.rs

#![forbid(unsafe_code)]

use crate::PlaneId;

/// Eco‑restoration credit output; never used as control input.
#[derive(Debug, Clone, Copy)]
pub struct EcoCredit {
    pub value: f64,
}

impl EcoCredit {
    /// Mint credits according to:
    /// C_eco = (-ΔV) / (1 + |r_carbon|) * log10(1 + J_w),
    /// under all three constraints:
    /// - ΔV < 0 (strict stability improvement),
    /// - Non‑offsettable planes in compliance,
    /// - J_w > 0 (verified workload energy).
    pub fn mint(
        delta_v: f64,
        r_carbon: f64,
        jw: f64,
        non_offsettable_compliant: bool,
    ) -> Option<EcoCredit> {
        if delta_v >= 0.0 {
            return None;
        }
        if !non_offsettable_compliant {
            return None;
        }
        if jw <= 0.0 {
            return None;
        }
        let denom = 1.0 + r_carbon.abs();
        let scale = (-delta_v) / denom;
        let log_j = (1.0 + jw).log10();
        Some(EcoCredit {
            value: scale * log_j,
        })
    }
}
