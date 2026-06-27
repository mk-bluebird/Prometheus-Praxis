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
