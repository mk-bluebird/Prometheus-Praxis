// filename: prometheus-praxis/spine/src/eco_credit.rs

#![forbid(unsafe_code)]

/// EcoCredit is an output-only metric.
/// It MUST NOT be used to alter LyapunovWeights, RiskCoord values,
/// or any controller decisions; credits cannot offset physical risk.

/// Eco‑restoration credit output; never used as control input.
#[derive(Debug, Clone, Copy)]
pub struct EcoCredit {
    /// The computed credit value.
    pub value: f64,
}

impl EcoCredit {
    /// Mint credits according to:
    /// C_eco = (-ΔV) / (1 + |r_carbon|) * log10(1 + J_w),
    /// under all three constraints:
    /// - ΔV < 0 (strict stability improvement),
    /// - Non‑offsettable planes in compliance,
    /// - J_w > 0 (verified workload energy).
    ///
    /// Returns `None` if any constraint is violated.
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

#[cfg(test)]
mod tests {
    use super::EcoCredit;

    #[test]
    fn eco_credit_mints_only_when_constraints_hold() {
        // ΔV < 0, compliant, positive energy
        let c = EcoCredit::mint(-0.1, 0.05, 10.0, true);
        assert!(c.is_some());

        // ΔV >= 0 blocks minting
        assert!(EcoCredit::mint(0.0, 0.05, 10.0, true).is_none());

        // non-compliant nonoffsettable planes block minting
        assert!(EcoCredit::mint(-0.1, 0.05, 10.0, false).is_none());

        // zero energy blocks minting
        assert!(EcoCredit::mint(-0.1, 0.05, 0.0, true).is_none());
    }

    #[test]
    fn eco_credit_value_formula_correctness() {
        // Verify the formula: C_eco = (-ΔV) / (1 + |r_carbon|) * log10(1 + J_w)
        let delta_v = -0.2;
        let r_carbon = 0.1;
        let jw = 9.0; // log10(1+9) = log10(10) = 1.0
        let compliant = true;

        let credit = EcoCredit::mint(delta_v, r_carbon, jw, compliant).unwrap();
        let expected = (0.2 / (1.0 + 0.1)) * (10.0_f64).log10();
        assert!((credit.value - expected).abs() < 1e-10);
    }

    #[test]
    fn eco_credit_negative_energy_rejected() {
        assert!(EcoCredit::mint(-0.1, 0.05, -5.0, true).is_none());
    }

    #[test]
    fn eco_credit_positive_carbon_risk_reduces_credit() {
        let base = EcoCredit::mint(-0.1, 0.0, 10.0, true).unwrap();
        let with_risk = EcoCredit::mint(-0.1, 0.5, 10.0, true).unwrap();
        assert!(with_risk.value < base.value);
    }
}
