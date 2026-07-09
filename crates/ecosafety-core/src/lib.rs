// File: crates/ecosafety-core/src/lib.rs
// Edition 2024, rust-version = "1.85"

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

/// Normalized risk coordinates for the core ecosafety planes.
/// All fields are expected to be clamped in [0.0, 1.0] by upstream kernels.
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub struct RiskCoords {
    pub renergy:    f32,
    pub rhydraulic: f32,
    pub rbio:       f32,
    pub rmaterials: f32,
    pub rcarbon:    f32,
    pub rtox:       f32,
    pub rmicro:     f32,
    pub rcalib:     f32,
    pub rsigma:     f32,
}

/// Tree-of-Life aligned weights for the Lyapunov residual.
/// Higher weights correspond to children/future humans and ecosystems.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct RiskWeights {
    pub wenergy:    f64,
    pub whydraulic: f64,
    pub wbio:       f64,
    pub wmaterials: f64,
    pub wcarbon:    f64,
    pub wtox:       f64,
    pub wmicro:     f64,
    pub wcalib:     f64,
    pub wsigma:     f64,
}

/// Canonical Phoenix Tree-of-Life weight vector v1.
/// Matches the documented semantics: biotoxmicro > hydraulics/materials/carbon > energy > calib/sigma.
pub const TREE_OF_LIFE_WEIGHTS_PHX_V1: RiskWeights = RiskWeights {
    wenergy:    0.8,
    whydraulic: 1.0,
    wbio:       1.5,
    wmaterials: 1.0,
    wcarbon:    1.0,
    wtox:       1.5,
    wmicro:     1.2,
    wcalib:     0.6,
    wsigma:     0.6,
};

/// Lyapunov residual V_t = sum_j w_j r_j^2 over the core planes.
/// r_j are normalized risk coordinates in [0,1], w_j >= 0.
pub fn lyapunov_residual(r: RiskCoords, w: RiskWeights) -> f64 {
    fn sq(x: f32) -> f64 {
        let y = x as f64;
        y * y
    }

    w.wenergy    * sq(r.renergy)
        + w.whydraulic * sq(r.rhydraulic)
        + w.wbio       * sq(r.rbio)
        + w.wmaterials * sq(r.rmaterials)
        + w.wcarbon    * sq(r.rcarbon)
        + w.wtox       * sq(r.rtox)
        + w.wmicro     * sq(r.rmicro)
        + w.wcalib     * sq(r.rcalib)
        + w.wsigma     * sq(r.rsigma)
}

/// Window-level KER metrics:
/// K = fraction of steps with non-increasing V_t (discrete Lyapunov safe-step),
/// R = maximum coordinate seen across all planes,
/// E = 1 - R (eco-impact).
pub fn ker_window(residuals: &[f64], coords: &[RiskCoords]) -> (f32, f32, f32) {
    assert_eq!(residuals.len(), coords.len());

    if residuals.is_empty() {
        return (0.0, 0.0, 1.0);
    }

    // Knowledge factor: fraction of steps where V_{t+1} <= V_t
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

    // Risk-of-harm: maximum coordinate seen over all planes
    let mut r_max = 0.0f32;
    for c in coords {
        r_max = r_max
            .max(c.renergy)
            .max(c.rhydraulic)
            .max(c.rbio)
            .max(c.rmaterials)
            .max(c.rcarbon)
            .max(c.rtox)
            .max(c.rmicro)
            .max(c.rcalib)
            .max(c.rsigma);
    }
    let r = r_max;
    let e = 1.0f32 - r.clamp(0.0, 1.0);

    (k, e, r)
}

/// Hydraulic + uncertainty sub-channel; useful when working specifically on hydraulics
/// but still wanting canonical weights.
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub struct HydraulicRiskCoords {
    pub rhydraulics: f32,
    pub rcalib:      f32,
    pub rsigma:      f32,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct HydraulicWeights {
    pub whydraulic: f64,
    pub wcalib:     f64,
    pub wsigma:     f64,
}

pub const HYDRAULIC_WEIGHTS_PHX_V1: HydraulicWeights = HydraulicWeights {
    whydraulic: 1.0,
    wcalib:     0.6,
    wsigma:     0.6,
};

pub fn vt_hydraulics(r: HydraulicRiskCoords, w: HydraulicWeights) -> f64 {
    let rh = r.rhydraulics as f64;
    let rc = r.rcalib as f64;
    let rs = r.rsigma as f64;

    w.whydraulic * rh * rh
        + w.wcalib * rc * rc
        + w.wsigma * rs * rs
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tree_of_life_prioritizes_bio_over_carbon() {
        let w = TREE_OF_LIFE_WEIGHTS_PHX_V1;
        let mut r = RiskCoords::default();

        // 0.5 bio risk
        r.rbio = 0.5;
        let v_bio = lyapunov_residual(r, w);

        // 0.5 carbon risk instead
        r.rbio = 0.0;
        r.rcarbon = 0.5;
        let v_carbon = lyapunov_residual(r, w);

        assert!(v_bio > v_carbon);
    }

    #[test]
    fn ker_window_matches_non_increase_in_residuals() {
        let w = TREE_OF_LIFE_WEIGHTS_PHX_V1;
        let coords = vec![
            RiskCoords { rbio: 0.4, ..RiskCoords::default() },
            RiskCoords { rbio: 0.3, ..RiskCoords::default() },
            RiskCoords { rbio: 0.2, ..RiskCoords::default() },
        ];
        let residuals: Vec<f64> = coords.iter().map(|c| lyapunov_residual(*c, w)).collect();

        let (k, e, r) = ker_window(&residuals, &coords);
        assert!((k - 1.0).abs() < 1e-6);
        assert!(e >= 0.0);
        assert!(r <= 0.4 + 1e-6);
    }

    #[test]
    fn hydraulic_vt_respects_calib_and_sigma() {
        let w = HYDRAULIC_WEIGHTS_PHX_V1;
        let r = HydraulicRiskCoords {
            rhydraulics: 0.2,
            rcalib:      0.8,
            rsigma:      0.5,
        };
        let v = vt_hydraulics(r, w);
        assert!(v > 0.0);
    }
}
