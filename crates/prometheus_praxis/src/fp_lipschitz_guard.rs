// Non-actuating guard: checks a(x, t) drift Lipschitz condition for discrete Fokker–Planck moves.

#![forbid(unsafe_code)]
#![cfg_attr(not(test), no_std)]

pub struct DriftParams {
    /// Proven Lipschitz constant L loaded from ALN spec.
    pub lipschitz_const: f64,
}

/// State vector in the deploy kernel's discrete grid.
#[derive(Clone, Copy)]
pub struct StateVec {
    pub x: f64,
    // Extend with more dimensions as needed (e.g., eco, RoH, Lifeforce).
}

/// Host-agnostic drift function signature.
pub type DriftFn = fn(StateVec, f64) -> f64;

pub struct FpLipschitzGuard {
    params: DriftParams,
}

impl FpLipschitzGuard {
    pub fn new(params: DriftParams) -> Self {
        Self { params }
    }

    /// Check Lipschitz continuity for a pair of states at fixed time t.
    /// Returns true iff |a(x1,t) - a(x2,t)| <= L * |x1 - x2|.
    pub fn check_pair(&self, a: DriftFn, s1: StateVec, s2: StateVec, t: f64) -> bool {
        let ax1 = a(s1, t);
        let ax2 = a(s2, t);
        let diff_a = (ax1 - ax2).abs();
        let diff_x = (s1.x - s2.x).abs();
        if diff_x == 0.0 {
            // Identical state: drift difference must be zero for strict Lipschitz, allow epsilon.
            diff_a <= 1e-12
        } else {
            diff_a <= self.params.lipschitz_const * diff_x
        }
    }
}
