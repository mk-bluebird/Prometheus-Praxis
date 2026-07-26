// filename: lib.rs
// destination: crates/blast_radius_kernel/src/lib.rs

#![forbid(unsafe_code)]

pub mod model;
pub mod eco_weight;
pub mod lambda_compute;
pub mod ffi;

pub use crate::model::{LambdaSummary, LambdaQuery};
pub use crate::lambda_compute::compute_lambda_for_segment;
pub use crate::ffi::{
    eco_lambda_for_segment_json,
    eco_lambda_for_region_json,
    eco_blast_radius_free_cstring,
};

/// Compute blast radius for a given segment and return a BlastRadius result.
/// Stub implementation with placeholder return value.
/// 
/// # Note on blastradius dependency
/// The blastradius crate should be added to Cargo.toml as:
/// ```toml
/// [dependencies]
/// blastradius = { path = "../blastradius" }  # or appropriate path/version
/// ```
/// This comment documents the intended dependency without modifying Cargo.toml.
pub fn compute_blast_radius(segment_id: &str, lambda: f64) -> blastradius::BlastRadius {
    // TODO: Implement full blast radius computation logic
    // This stub returns a placeholder BlastRadius value.
    blastradius::BlastRadius {
        segment_id: segment_id.to_string(),
        radius_meters: 0.0,
        confidence: 0.0,
    }
}
