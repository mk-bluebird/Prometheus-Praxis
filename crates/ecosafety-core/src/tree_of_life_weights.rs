// File: crates/ecosafety-core/src/tree_of_life_weights.rs
// Edition: 2024, Rust >= 1.85
// License: MIT OR Apache-2.0

//! Tree-of-life aligned RiskWeights and Lyapunov residual.
//! Children/future humans + aquatic/soil life via bio/tox/micro planes,
//! then hydraulics/materials/carbon, then energy and data-quality.
//! Aligned with ecosafety spine, FOG, and FlowVac work. [file:23][file:24][file:25][file:17]

#![forbid(unsafe_code)]

/// Normalized risk coordinates.
/// All fields are expected in [0.0, 1.0].
#[derive(Clone, Copy, Debug, Default)]
pub struct RiskCoords {
    pub r_energy: f32,
    pub r_hydraulic: f32,
    pub r_bio: f32,
    pub r_materials: f32,
    pub r_carbon: f32,
    pub r_tox: f32,
    pub r_micro: f32,
    pub r_calib: f32,
    pub r_sigma: f32,
}

/// Tree-of-life aligned weights for the Lyapunov residual.
/// High weights correspond to children/future humans and ecosystems. [file:23][file:25]
#[derive(Clone, Copy, Debug)]
pub struct RiskWeights {
    pub w_energy: f64,
    pub w_hydraulic: f64,
    pub w_bio: f64,
    pub w_materials: f64,
    pub w_carbon: f64,
    pub w_tox: f64,
    pub w_micro: f64,
    pub w_calib: f64,
    pub w_sigma: f64,
}

/// Canonical Phoenix Tree-of-life weights (v1).
pub const TREE_OF_LIFE_WEIGHTS_V1: RiskWeights = RiskWeights {
    // Energy drift and tailwind plane. [file:22][file:23]
    w_energy: 0.8,
    // Hydraulics (SAT corridors, surcharge). [file:22][file:23]
    w_hydraulic: 1.0,
    // Children / future humans + aquatic/soil life via bio/tox/micro. [file:23][file:25]
    w_bio: 1.5,
    w_tox: 1.5,
    w_micro: 1.2,
    // Materials and carbon planes. [file:23][file:25]
    w_materials: 1.0,
    w_carbon: 1.0,
    // Data-quality and model-uncertainty planes. [file:23][file:25][file:17]
    w_calib: 0.6,
    w_sigma: 0.6,
};

/// Compute Lyapunov residual V for given coords and weights.
/// V = sum_j w_j * r_j^2, with r_j ∈ [0,1].
pub fn lyapunov_residual(r: &RiskCoords, w: &RiskWeights) -> f64 {
    let sq = |x: f32| {
        let y = x as f64;
        y * y
    };
    w.w_energy * sq(r.r_energy)
        + w.w_hydraulic * sq(r.r_hydraulic)
        + w.w_bio * sq(r.r_bio)
        + w.w_materials * sq(r.r_materials)
        + w.w_carbon * sq(r.r_carbon)
        + w.w_tox * sq(r.r_tox)
        + w.w_micro * sq(r.r_micro)
        + w.w_calib * sq(r.r_calib)
        + w.w_sigma * sq(r.r_sigma)
}

/// Eco-wealth aligned KER window calculation for a short trajectory.
/// K = fraction of steps with non-increasing V,
/// R = max coordinate seen, E = 1 - R. [file:23][file:24]
pub fn ker_window(
    residuals: &[f64],
    coords: &[RiskCoords],
) -> (f32, f32, f32) {
    assert_eq!(residuals.len(), coords.len());
    if residuals.is_empty() {
        return (0.0, 0.0, 1.0);
    }
    let mut safe_steps = 0usize;
    for win in residuals.windows(2) {
        if win[1] <= win[0] {
            safe_steps += 1;
        }
    }
    let k = if residuals.len() > 1 {
        safe_steps as f32 / (residuals.len() as f32 - 1.0)
    } else {
        1.0
    };

    let mut r_max = 0.0_f32;
    for c in coords {
        r_max = r_max
            .max(c.r_energy)
            .max(c.r_hydraulic)
            .max(c.r_bio)
            .max(c.r_materials)
            .max(c.r_carbon)
            .max(c.r_tox)
            .max(c.r_micro)
            .max(c.r_calib)
            .max(c.r_sigma);
    }
    let r = r_max;
    let e = 1.0_f32 - r.clamp(0.0, 1.0);
    (k, e, r)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tree_of_life_weights_prioritize_bio_over_carbon() {
        let w = TREE_OF_LIFE_WEIGHTS_V1;
        let mut r = RiskCoords::default();
        r.r_bio = 0.5;
        let v_bio = lyapunov_residual(&r, &w);
        r.r_bio = 0.0;
        r.r_carbon = 0.5;
        let v_carbon = lyapunov_residual(&r, &w);
        assert!(v_bio > v_carbon);
    }

    #[test]
    fn ker_window_matches_lyapunov_non_increase() {
        let w = TREE_OF_LIFE_WEIGHTS_V1;
        let coords = vec![
            RiskCoords { r_bio: 0.4, ..Default::default() },
            RiskCoords { r_bio: 0.3, ..Default::default() },
            RiskCoords { r_bio: 0.2, ..Default::default() },
        ];
        let residuals: Vec<f64> =
            coords.iter().map(|c| lyapunov_residual(c, &w)).collect();
        let (k, e, r) = ker_window(&residuals, &coords);
        assert!((k - 1.0).abs() < 1e-6);
        assert!(e > 0.0 && r <= 0.5);
    }
}
