// crates/volatility-guard/src/non_offsettable_plane.rs

pub trait EcoPlane {
    /// Returns the current scalar coordinate in [0, 1].
    fn coordinate(&self) -> f32;

    /// Returns true if this coordinate is strictly worse (higher risk)
    /// than in `other`, under your corridor semantics.
    fn strictly_degrades(&self, other: &dyn EcoPlane) -> bool;
}

/// Marker for planes that must never be "paid down" by improvements elsewhere.
pub trait NonOffsettablePlane: EcoPlane {
    /// True if this plane is currently beyond its hard band / corridor.
    fn hard_band_breach(&self) -> bool;

    /// True if this plane cannot be compensated by `other`:
    /// either because `self` is non‑offsettable or because any
    /// attempted tradeoff is prohibited by your eco grammar.
    fn never_compensated_by(&self, other: &dyn EcoPlane) -> bool;
}
