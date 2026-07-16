// File: Prometheus-Praxis/rust/eco-wealth-kernel/src/lib.rs

use std::os::raw::c_double;
use std::os::raw::c_int;

#[no_mangle]
pub extern "C" fn eco_wealth_compute(
    alpha: c_double,
    beta: c_double,
    gamma: c_double,
    k: c_double,
    e: c_double,
    r: c_double,
) -> c_double {
    // Example eco-wealth function: W = K^alpha * E^beta / (1.0 + R^gamma).
    let k_term = k.powf(alpha);
    let e_term = e.powf(beta);
    let r_term = r.powf(gamma);
    k_term * e_term / (1.0 + r_term)
}

/// Returns 1 if the given plane is non-compensable, 0 otherwise.
/// Planes are encoded as integers, e.g., 0=K,1=E,2=R.
#[no_mangle]
pub extern "C" fn eco_is_noncompensable_plane(plane_id: c_int) -> c_int {
    // Example noncompensableplanes = {1, 2} meaning E and R planes.
    match plane_id {
        1 | 2 => 1,
        _ => 0,
    }
}
