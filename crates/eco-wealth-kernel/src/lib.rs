// File: Prometheus-Praxis/rust/eco-wealth-kernel/src/lib.rs

use std::os::raw::{c_double, c_float, c_int, c_ulong};

#[link(name = "lyapunov_residual_simd")]
extern "C" {
    fn lyapunov_residual_simd(
        weights: *const c_float,
        risks: *const c_float,
        len: c_ulong,
    ) -> c_double;
}

#[no_mangle]
pub extern "C" fn eco_wealth_compute(
    alpha: c_double,
    beta: c_double,
    gamma: c_double,
    k: c_double,
    e: c_double,
    r: c_double,
) -> c_double {
    let k_clamped = if k < 0.0 { 0.0 } else if k > 1.0 { 1.0 } else { k };
    let e_clamped = if e < 0.0 { 0.0 } else if e > 1.0 { 1.0 } else { e };
    let r_clamped = if r < 0.0 { 0.0 } else if r > 1.0 { 1.0 } else { r };

    let k_term = k_clamped.powf(alpha);
    let e_term = e_clamped.powf(beta);
    let r_term = r_clamped.powf(gamma);

    if (1.0 + r_term).is_finite() && (1.0 + r_term) != 0.0 {
        k_term * e_term / (1.0 + r_term)
    } else {
        0.0
    }
}

pub fn lyapunov_residual(weights: &[f32], risks: &[f32]) -> f64 {
    assert_eq!(weights.len(), risks.len());
    unsafe {
        lyapunov_residual_simd(
            weights.as_ptr(),
            risks.as_ptr(),
            weights.len() as c_ulong,
        )
    }
}

#[no_mangle]
pub extern "C" fn eco_is_noncompensable_plane(plane_id: c_int) -> c_int {
    match plane_id {
        1 | 2 => 1,
        _ => 0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn eco_wealth_basic_monotone() {
        let w1 = eco_wealth_compute(1.0, 1.0, 1.0, 0.8, 0.9, 0.2);
        let w2 = eco_wealth_compute(1.0, 1.0, 1.0, 0.9, 0.9, 0.2);
        assert!(w2 >= w1);
    }

    #[test]
    fn eco_is_noncompensable_plane_flags_e_r() {
        assert_eq!(eco_is_noncompensable_plane(1), 1);
        assert_eq!(eco_is_noncompensable_plane(2), 1);
        assert_eq!(eco_is_noncompensable_plane(0), 0);
    }
}
