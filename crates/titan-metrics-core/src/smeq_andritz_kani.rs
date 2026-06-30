// filename: titan-metrics-core/src/smeq_andritz_kani.rs
// destination: titan-net/crates/titan-metrics-core/src/smeq_andritz_kani.rs

#![forbid(unsafe_code)]

pub struct SmeQJacobian {
    pub modeltag: &'static str,
    pub a11: f64,
    pub a12: f64,
    pub a21: f64,
    pub a22: f64,
    pub l_mu_eff: f64,
}

pub struct SmeQLyapWeights {
    pub a1: f64,
    pub a2: f64,
    pub x_min_norm: f64,
}

pub fn eigenvalues(j: &SmeQJacobian) -> (f64, f64, f64, f64) {
    let tr = j.a11 + j.a22;
    let det = j.a11 * j.a22 - j.a12 * j.a21;
    let disc = tr * tr - 4.0 * det;

    if disc >= 0.0 {
        let root = disc.sqrt();
        let lambda1 = 0.5 * (tr + root);
        let lambda2 = 0.5 * (tr - root);
        (lambda1, 0.0, lambda2, 0.0)
    } else {
        let root = (-disc).sqrt();
        let re = 0.5 * tr;
        let im = 0.5 * root;
        (re, im, re, -im)
    }
}

pub fn max_mu_excursion(j: &SmeQJacobian, w: &SmeQLyapWeights) -> f64 {
    let (lambda1_re, _, lambda2_re, _) = eigenvalues(j);
    let alpha_min = -lambda1_re.min(lambda2_re);
    let a_max = w.a1.max(w.a2);
    let c_max = (alpha_min * w.x_min_norm) / (2.0 * a_max);
    c_max / j.l_mu_eff
}

// Kani harness: check_kf_monotonic_invariant
#[cfg(kani)]
mod invariants {
    use super::*;

    #[kani::proof]
    fn check_kf_monotonic_andritz_smeq_1450() {
        let j = SmeQJacobian {
            modeltag: "ANDRITZ-SMEQ-1450",
            a11: -0.15,
            a12: 0.04,
            a21: -0.03,
            a22: -0.20,
            l_mu_eff: 0.35,
        };

        let w = SmeQLyapWeights {
            a1: 2.0,
            a2: 1.0,
            x_min_norm: 0.02,
        };

        let delta_mu_max = max_mu_excursion(&j, &w);

        // RoH ceiling invariant: viscosity drift must not exceed 0.0025 Pa·s
        kani::assume(delta_mu_max >= 0.0);
        kani::assert(delta_mu_max <= 0.0025);
    }
}
