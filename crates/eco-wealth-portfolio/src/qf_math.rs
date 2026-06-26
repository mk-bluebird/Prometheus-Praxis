// filename: crates/eco-wealth-portfolio/src/qf_math.rs

use rust_decimal::Decimal;
use rust_decimal::prelude::*;
use rust_decimal_macros::dec;
use serde::{Deserialize, Serialize};

/// ResponsibilityScalar ∈ [0,1], governance‑set per contributor.
///
/// Higher values mean more trusted / accountable behavior.
/// This is where KER / neurorights history can be folded in.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct ResponsibilityScalar(pub Decimal);

impl ResponsibilityScalar {
    pub fn new(value: Decimal) -> Self {
        let v = value.clamp(dec!(0), dec!(1));
        ResponsibilityScalar(v)
    }
}

/// EcoCreditAttenuation encodes cumulative eco‑credits and domain alpha.
///
/// `alpha` controls how fast marginal influence attenuates as cumulative
/// eco‑credits grow.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct EcoCreditAttenuation {
    pub alpha: Decimal,
    pub cumulative_credits: Decimal,
}

impl EcoCreditAttenuation {
    pub fn new(alpha: Decimal, cumulative_credits: Decimal) -> Self {
        let a = if alpha < dec!(0) { dec!(0) } else { alpha };
        let c = if cumulative_credits < dec!(0) {
            dec!(0)
        } else {
            cumulative_credits
        };
        EcoCreditAttenuation {
            alpha: a,
            cumulative_credits: c,
        }
    }
}

/// Deterministic sqrt for `Decimal` using a short Newton iteration.
///
/// Assumes `x >= 0`. Returns 0 for negative inputs.
pub fn decimal_sqrt(x: Decimal) -> Decimal {
    if x <= dec!(0) {
        return dec!(0);
    }

    // Initial guess via f64 bridge.
    let x_f64 = x.to_f64().unwrap_or(0.0);
    if x_f64 <= 0.0 {
        return dec!(0);
    }
    let mut guess = Decimal::from_f64(x_f64.sqrt()).unwrap_or(dec!(0));

    // If guess dropped to 0 due to precision, fall back to x/2.
    if guess <= dec!(0) {
        guess = x / dec!(2);
    }

    // A few Newton iterations: g_{n+1} = (g_n + x / g_n) / 2
    for _ in 0..8 {
        if guess <= dec!(0) {
            break;
        }
        let next = (guess + x / guess) / dec!(2);
        // Stop early if converged.
        if (next - guess).abs() < dec!(1e-12) {
            guess = next;
            break;
        }
        guess = next;
    }

    if guess < dec!(0) {
        dec!(0)
    } else {
        guess
    }
}

/// Core per‑contribution QF weight.
///
/// Formula:
///   1. Clamp `responsibility` into \([0,1]\).
///   2. Compute attenuation factor:
///        atten = 1 / (1 + alpha * cumulative_credits)
///      with `alpha >= 0`, `cumulative_credits >= 0`.
///   3. Effective amount:
///        effective = contribution_amount * responsibility * atten
///   4. Weight:
///        w = sqrt(effective)
///
/// All arithmetic is in `Decimal`; the only f64 bridge is in `decimal_sqrt`.
pub fn compute_qf_weight(
    contribution_amount: Decimal,
    responsibility: ResponsibilityScalar,
    attenuation: EcoCreditAttenuation,
) -> Decimal {
    if contribution_amount <= dec!(0) {
        return dec!(0);
    }

    let r = responsibility.0.clamp(dec!(0), dec!(1));

    let alpha = if attenuation.alpha < dec!(0) {
        dec!(0)
    } else {
        attenuation.alpha
    };
    let credits = if attenuation.cumulative_credits < dec!(0) {
        dec!(0)
    } else {
        attenuation.cumulative_credits
    };

    let base = dec!(1) + alpha * credits;
    let atten_factor = if base > dec!(0) {
        dec!(1) / base
    } else {
        dec!(0)
    };

    let effective = contribution_amount * r * atten_factor;
    if effective <= dec!(0) {
        return dec!(0);
    }

    decimal_sqrt(effective)
}

/// Compute QF total \( (\sum_i \sqrt{c_i})^2 \) from pre‑attenuated effective amounts.
///
/// Invariants:
/// - Uses `Decimal` throughout, with a single f64 round‑trip at the end.
/// - Applies per‑term and aggregate corridor clamps to avoid overflow.
pub fn compute_qf_total(effective_amounts: &[Decimal]) -> Decimal {
    let mut sum_roots = dec!(0);

    // Governance‑defined cap per effective amount to avoid pathological inputs.
    let max_effective = Decimal::from_i128_with_scale(1_000_000_000_000, 6); // 1e6 with 6dp

    for amt in effective_amounts {
        if *amt <= dec!(0) {
            continue;
        }
        let capped = if *amt > max_effective {
            max_effective
        } else {
            *amt
        };
        let root = decimal_sqrt(capped);
        sum_roots += root;
    }

    // Aggregate corridor: cap sum_roots before squaring.
    let max_sum_roots = Decimal::from_i128_with_scale(1_000_000_000_000, 6);
    let clamped = if sum_roots > max_sum_roots {
        max_sum_roots
    } else {
        sum_roots
    };

    // Final square via f64 bridge; still bounded by corridor caps.
    let s_f64 = clamped.to_f64().unwrap_or(0.0);
    let qf_f64 = s_f64 * s_f64;
    Decimal::from_f64(qf_f64).unwrap_or(dec!(0))
}

#[cfg(test)]
mod tests {
    use super::*;
    use rust_decimal_macros::dec;

    #[test]
    fn test_decimal_sqrt_basic() {
        let x = dec!(4);
        let r = decimal_sqrt(x);
        assert!((r - dec!(2)).abs() < dec!(1e-9));
    }

    #[test]
    fn test_compute_qf_weight_monotone_in_amount() {
        let r = ResponsibilityScalar::new(dec!(1));
        let att = EcoCreditAttenuation::new(dec!(0), dec!(0));

        let w1 = compute_qf_weight(dec!(1), r, att);
        let w4 = compute_qf_weight(dec!(4), r, att);
        assert!(w4 > w1);
    }

    #[test]
    fn test_compute_qf_total_simple() {
        let r = ResponsibilityScalar::new(dec!(1));
        let att = EcoCreditAttenuation::new(dec!(0), dec!(0));
        let w1 = compute_qf_weight(dec!(1), r, att);
        let w4 = compute_qf_weight(dec!(4), r, att);

        let total = compute_qf_total(&[w1, w4]);
        // Expected: (sqrt(1) + sqrt(4))^2 = (1 + 2)^2 = 9
        assert!((total - dec!(9)).abs() < dec!(1e-6));
    }
}
