// File: crates/volatility-guard/src/non_offsettable_plane.rs
pub trait EcoPlane {
    fn coordinate(&self) -> f64;
    fn strictly_degrades(&self, other: &dyn EcoPlane) -> bool;
}

pub trait NonOffsettablePlane: EcoPlane {
    fn hard_band_breach(&self) -> bool;
    fn never_compensated_by(&self, other: &dyn EcoPlane) -> bool;
}
