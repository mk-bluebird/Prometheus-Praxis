// filename: crates/cyboquatic-core/src/lib.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

//! Cyboquatic core recognition and indexing crate.
//!
//! This crate is explicitly non-actuating. It provides:
//! - Per-node eco/energy recognition (`CyboquaticNodeSample` → `CyboquaticEcoPlot`).
//! - Restoration surfaces and carbon-negative flags for frames and orchestration.
//! - Region-level aggregations and GeoJSON emitters for overlays.
//! - Synthetic Phoenix `NodeRiskSample` and ESPD-style ecosafety scoring.
//! - A `FrameRegistry` driven by `Frames.toml` to enable or disable diagnostics.
//! - An optional `metrics` feature exporting Prometheus-style diagnostics.
//!
//! Any controller or actuator using these outputs must still pass through ALN
//! `safesteprule` and `deploydecisionkernel` gates in higher layers.

mod cyboquatic_index;
mod frame_registry;
mod metrics;

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

pub use crate::frame_registry::{FrameKind, FrameRegistry};
pub use crate::metrics::{export_last_metrics, record_metrics_snapshot, MetricsSnapshot};
