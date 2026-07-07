// filename: cyboquatic-core/examples/cyboquatic_region_geojson.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

use cyboquatic_core::{
    aggregate_by_region, build_cyboquatic_index, emit_region_geojson, CyboquaticNodeSample,
};
use serde_json::json;
use std::collections::HashMap;

/// Example binary:
/// 1. Reads CyboquaticNodeSample rows from stdin as JSON lines.
/// 2. Builds CyboquaticIndex and region aggregates.
/// 3. Emits a GeoJSON FeatureCollection to stdout for CyboquaticOverlay.kt.
///
/// This example is non-actuating: it only computes recognition surfaces.
fn main() {
    // For a minimal example, we synthesize a small set of nodes.
    // In production, replace this with JSONL / CSV ingestion wired via ecoshard.
    let samples = vec![
        CyboquaticNodeSample {
            node_id: "PHX-001".to_string(),
            region: "Phoenix-Central".to_string(),
            avg_power_w: 120.0,
            energy_j: 1.2e6,
            co2e_kg: -5.0,
            ecosafety_risk: 0.18,
            eco_benefit: 0.85,
            restoration_radius_m: Some(250.0),
            meta: HashMap::new(),
        },
        CyboquaticNodeSample {
            node_id: "PHX-002".to_string(),
            region: "Phoenix-Central".to_string(),
            avg_power_w: 95.0,
            energy_j: 9.5e5,
            co2e_kg: 2.0,
            ecosafety_risk: 0.22,
            eco_benefit: 0.70,
            restoration_radius_m: Some(200.0),
            meta: HashMap::new(),
        },
        CyboquaticNodeSample {
            node_id: "PHX-003".to_string(),
            region: "Phoenix-West".to_string(),
            avg_power_w: 80.0,
            energy_j: 8.0e5,
            co2e_kg: -1.5,
            ecosafety_risk: 0.15,
            eco_benefit: 0.60,
            restoration_radius_m: Some(180.0),
            meta: HashMap::new(),
        },
    ];

    let index = build_cyboquatic_index(samples);
    let aggregates = aggregate_by_region(&index);

    // Minimal GeoJSON geometries for the example: replace with actual polygons
    // from qpudatashards GeoJSON shards in real deployments.
    let mut region_geometries: HashMap<String, serde_json::Value> = HashMap::new();
    region_geometries.insert(
        "Phoenix-Central".to_string(),
        json!({
            "type": "Point",
            "coordinates": [-112.07, 33.45]
        }),
    );
    region_geometries.insert(
        "Phoenix-West".to_string(),
        json!({
            "type": "Point",
            "coordinates": [-112.20, 33.47]
        }),
    );

    let fc = emit_region_geojson(&aggregates, &region_geometries);
    let out = serde_json::to_string_pretty(&fc).expect("serialize GeoJSON");
    println!("{out}");
}
