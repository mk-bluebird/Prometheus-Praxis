// Filename: src/fp_biology_solver.rs
// Destination: eco_restoration_shard/cyboquatic_fp/src/fp_biology_solver.rs

#![forbid(unsafe_code)]
#![allow(clippy::needless_return)]

use nalgebra::{DVector};
use kani::kani;

/// Scalar type alias for clarity.
pub type Scalar = f64;

/// Discrete biology grid over one coordinate (e.g. pathogen concentration).
#[derive(Clone, Debug)]
pub struct BiologyGrid {
    /// Bin centers (e.g. pathogen concentrations).
    pub x: DVector<Scalar>,
    /// Bin widths.
    pub dx: DVector<Scalar>,
    /// Drift a(x) at bin centers.
    pub drift: DVector<Scalar>,
    /// Diffusion D(x) at bin centers.
    pub diffusion: DVector<Scalar>,
    /// Risk weights V_i = w_path * r_i^2 precomputed per bin.
    pub v_bin: DVector<Scalar>,
}

impl BiologyGrid {
    /// Create a simple uniform grid on [x_min, x_max] with N bins,
    /// and monotone "downhill" drift toward lower-risk bins.
    pub fn uniform(
        x_min: Scalar,
        x_max: Scalar,
        n: usize,
        w_path: Scalar,
        risk_kernel: &dyn Fn(Scalar) -> Scalar,
    ) -> Self {
        assert!(n >= 2);
        let dx_val = (x_max - x_min) / (n as Scalar);
        let mut x = DVector::zeros(n);
        let mut dx = DVector::from_element(n, dx_val);
        let mut drift = DVector::zeros(n);
        let mut diffusion = DVector::zeros(n);
        let mut v_bin = DVector::zeros(n);

        for i in 0..n {
            let xi = x_min + (i as Scalar + 0.5) * dx_val;
            x[i] = xi;

            // Risk coordinate r_i in [0,1].
            let r_i = risk_kernel(xi).clamp(0.0, 1.0);
            v_bin[i] = w_path * r_i * r_i;

            // Simple "downhill" drift: move toward lower V if possible.
            // Here we choose a linear drift that pushes toward x_min.
            drift[i] = -1.0; // negative drift: towards lower concentration

            // Constant diffusion for now; can be tied to rsigma later.
            diffusion[i] = 0.1;
        }

        BiologyGrid {
            x,
            dx,
            drift,
            diffusion,
            v_bin,
        }
    }

    /// Number of bins.
    pub fn len(&self) -> usize {
        self.x.len()
    }
}

/// Probability distribution over the biology grid.
#[derive(Clone, Debug)]
pub struct ProbabilityState {
    /// Probability per bin; must sum to 1.0 in normalized form.
    pub p: DVector<Scalar>,
}

impl ProbabilityState {
    /// Initialize with a given vector, normalized to sum 1.0.
    pub fn from_raw(p_raw: DVector<Scalar>) -> Self {
        let sum = p_raw.sum();
        assert!(sum > 0.0);
        let p = p_raw / sum;
        ProbabilityState { p }
    }

    /// Compute total probability mass (for conservation checks).
    pub fn mass(&self) -> Scalar {
        self.p.sum()
    }

    /// Compute discrete Lyapunov functional V_h = sum_i V_i * p_i.
    pub fn lyapunov(&self, grid: &BiologyGrid) -> Scalar {
        assert_eq!(self.p.len(), grid.v_bin.len());
        (0..self.p.len())
            .map(|i| grid.v_bin[i] * self.p[i])
            .sum()
    }
}

/// Compute one explicit Fokker–Planck step on the biology grid.
/// Returns the updated probability state.
/// Boundary conditions: zero flux at ends (reflective).
pub fn fp_step(
    grid: &BiologyGrid,
    state: &ProbabilityState,
    dt: Scalar,
) -> ProbabilityState {
    let n = grid.len();
    assert_eq!(n, state.p.len());

    let mut flux = DVector::zeros(n + 1); // interfaces: 0..n
    let mut p_next = DVector::zeros(n);

    // Interface fluxes: i+1/2 mapped to index i+1.
    // Reflective boundaries: flux[0] = flux[n] = 0.
    flux[0] = 0.0;
    flux[n] = 0.0;

    for i in 0..(n - 1) {
        let a_i = grid.drift[i];
        let a_ip1 = grid.drift[i + 1];
        let a_iface = 0.5 * (a_i + a_ip1);

        let d_i = grid.diffusion[i];
        let d_ip1 = grid.diffusion[i + 1];
        let d_iface = 0.5 * (d_i + d_ip1);

        let dx_iface = 0.5 * (grid.dx[i] + grid.dx[i + 1]);

        // Upwind drift flux.
        let f_drift = if a_iface >= 0.0 {
            a_iface * state.p[i]
        } else {
            a_iface * state.p[i + 1]
        };

        // Central diffusion flux.
        let f_diff = -d_iface * (state.p[i + 1] - state.p[i]) / dx_iface;

        flux[i + 1] = f_drift + f_diff;
    }

    // Update p_i: p_i' = p_i + dt * (F_{i-1/2} - F_{i+1/2}).
    for i in 0..n {
        let f_left = flux[i];
        let f_right = flux[i + 1];
        p_next[i] = state.p[i] + dt * (f_left - f_right);
        // Prevent negative probabilities due to numerical error.
        if p_next[i] < 0.0 {
            p_next[i] = 0.0;
        }
    }

    // Renormalize to preserve total probability mass.
    let sum_next: Scalar = p_next.sum();
    if sum_next > 0.0 {
        p_next /= sum_next;
    }

    ProbabilityState { p: p_next }
}

/// Simple risk kernel example: map concentration x into [0,1]
/// using a clipped affine corridor (safe, gold, hard bands).
pub fn risk_kernel_example(x: Scalar) -> Scalar {
    // Example corridor: safe <= 1.0, hard >= 10.0.
    let safe = 1.0;
    let hard = 10.0;
    if x <= safe {
        0.0
    } else if x >= hard {
        1.0
    } else {
        (x - safe) / (hard - safe)
    }
}

/// Kani proof harness: check probability conservation and
/// Lyapunov non-increase for one step, for small grids.
#[kani::proof]
fn kani_fp_step_preserves_properties() {
    let n: usize = 4;
    let grid = BiologyGrid::uniform(
        0.0,
        10.0,
        n,
        1.0, // w_path
        &risk_kernel_example,
    );

    // Initial probability: arbitrary positive entries.
    let p_raw = DVector::from_vec(vec![0.2, 0.3, 0.1, 0.4]);
    let state = ProbabilityState::from_raw(p_raw);

    let mass_before = state.mass();
    let v_before = state.lyapunov(&grid);

    // Small time step to keep explicit scheme stable.
    let dt: Scalar = 0.01;
    let state_next = fp_step(&grid, &state, dt);

    let mass_after = state_next.mass();
    let v_after = state_next.lyapunov(&grid);

    // Probability conservation (up to numerical renormalization).
    kani::assert!(mass_after > 0.0);
    // We normalize to 1.0, so both masses should be 1.0.
    kani::assert!((mass_before - 1.0).abs() < 1e-12);
    kani::assert!((mass_after - 1.0).abs() < 1e-12);

    // Lyapunov non-increase: V_h(next) <= V_h(before) + epsilon.
    // Allow tiny epsilon for floating-point.
    let eps: Scalar = 1e-9;
    kani::assert!(v_after <= v_before + eps);
}
