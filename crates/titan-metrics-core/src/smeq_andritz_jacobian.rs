// filename: titan-metrics-core/src/smeq_andritz_jacobian.rs
// destination: titan-net/crates/titan-metrics-core/src/smeq_andritz_jacobian.rs

#![forbid(unsafe_code)]

pub struct SmeQJacobian {
    pub modeltag: String,
    pub mu_nominal: f64,
    pub a11: f64,
    pub a12: f64,
    pub a21: f64,
    pub a22: f64,
    pub l_mu_eff: f64,
    pub mu_delta_max: f64,
}

pub struct Eigenpair {
    pub lambda1: f64,
    pub lambda2: f64,
}

/// Compute exact eigenvalues for the 2×2 Jacobian A(mu*).
pub fn eigenvalues(j: &SmeQJacobian) -> Eigenpair {
    let tr = j.a11 + j.a22;
    let det = j.a11 * j.a22 - j.a12 * j.a21;
    let disc = tr * tr - 4.0 * det;
    let root = if disc >= 0.0 { disc.sqrt() } else { 0.0 };
    let lambda1 = 0.5 * (tr + root);
    let lambda2 = 0.5 * (tr - root);
    Eigenpair { lambda1, lambda2 }
}
