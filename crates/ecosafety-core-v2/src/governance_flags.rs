// filename: crates/ecosafety-core-v2/src/governance_flags.rs

#![forbid(unsafe_code)]

pub type FlagLevel = u8; // 0,1,2 as described.

/// Zero-knowledge governance flag trait: implementors expose
/// blast-radius and health-proximity levels without leaking internal state.
pub trait GovernanceFlaggable {
    /// Blast-radius level B_f \in {0,1,2}.
    fn blast_radius_level(&self) -> FlagLevel;

    /// Health/reward proximity level H_f \in {0,1,2}.
    fn health_proximity_level(&self) -> FlagLevel;

    /// Aggregate governance flag G_f = max(B_f, H_f).
    fn governance_level(&self) -> FlagLevel {
        let b = self.blast_radius_level();
        let h = self.health_proximity_level();
        if b > h { b } else { h }
    }
}
