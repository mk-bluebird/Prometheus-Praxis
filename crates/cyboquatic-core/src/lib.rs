// filename: crates/cyboquatic-core/src/lib.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

//! Cyboquatic core recognition and indexing crate.
//!
//! This crate is explicitly **non-actuating**: it provides data structures and
//! pure functions for:
//! - Per-node eco/energy recognition (`CyboquaticNodeSample` → `CyboquaticEcoPlot`).
//! - Restoration surfaces and carbon-negative flags for frames and orchestration.
//! - Region-level aggregations suitable for GeoJSON heatmaps.
//! - Synthetic Phoenix `NodeRiskSample` generation and ESPD-style ecosafety scoring.
//!
//! Any controller or actuator using these outputs MUST still pass through ALN
//! `safesteprule` and `deploydecisionkernel` gates in higher layers.[file:32]

mod cyboquatic_index;

pub use crate::cyboquatic_index::{
    aggregate_by_region,
    build_cyboquatic_index,
    emit_region_geojson,
    espd_ecosafety_from_sample,
    make_phoenix_synthetic_sample,
    CyboquaticEcoPlot,
    CyboquaticIndex,
    CyboquaticNodeSample,
    CyboquaticRestorationSurface,
    CyboquaticWindowPlane,
    CyboquaticWindowWithPlanes,
    GeoJsonFeature,
    GeoJsonFeatureCollection,
    NodeRiskSample,
    RegionAggregate,
    Scalar,
    K_FACTOR,
    E_FACTOR,
    R_FACTOR,
};
