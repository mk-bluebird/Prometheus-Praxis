// File: crates/ecosafety-core/src/hydraulics.rs
// Edition 2024, rust-version = "1.85"

#[derive(Clone, Copy, Debug)]
pub struct HydraulicRiskCoords {
    pub rhydraulics: f32, // 0.0–1.0
    pub rcalib:      f32, // 0.0–1.0
    pub rsigma:      f32, // 0.0–1.0
}

#[derive(Clone, Copy, Debug)]
pub struct HydraulicWeights {
    pub whydraulic: f64,
    pub wcalib:     f64,
    pub wsigma:     f64,
}

// Phoenix Tree-of-Life weights for hydraulics + uncertainty
pub const HYDRAULIC_WEIGHTS_PHX_V1: HydraulicWeights = HydraulicWeights {
    whydraulic: 1.0,
    wcalib:     0.6,
    wsigma:     0.6,
};

pub fn vt_hydraulics(r: HydraulicRiskCoords, w: HydraulicWeights) -> f64 {
    let rh   = r.rhydraulics as f64;
    let rc   = r.rcalib as f64;
    let rs   = r.rsigma as f64;

    w.whydraulic * rh * rh
        + w.wcalib * rc * rc
        + w.wsigma * rs * rs
}
