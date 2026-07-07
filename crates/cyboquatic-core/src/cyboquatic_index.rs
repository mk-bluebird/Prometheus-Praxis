// filename: cyboquatic-core/src/cyboquatic_index.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};
use serde_json;
use std::collections::HashMap;

/// Scalar type for all ecokernel math in Cyboquatic space.
pub type Scalar = f64;

/// Knowledge / eco scores for this module.
/// K: knowledge factor, E: eco-impact benefit, R: risk-of-harm.
pub const K_FACTOR: Scalar = 0.96;
pub const E_FACTOR: Scalar = 0.93;
pub const R_FACTOR: Scalar = 0.11;

/// Core per-node input used by CyboquaticEcoPlot and restoration kernels.
/// This struct is intentionally non-actuating: it only represents telemetry and
/// derived recognition features.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CyboquaticNodeSample {
    /// Unique node identifier (stable within a basin / mesh).
    pub node_id: String,
    /// Region/group identifier (e.g. "Phoenix-Central", "CAP-Canal-Segment-7").
    pub region: String,
    /// Average node power draw in watts over the sampling window.
    pub avg_power_w: Scalar,
    /// Total energy in joules consumed over the sampling window.
    pub energy_j: Scalar,
    /// Estimated net CO₂e in kilograms attributable to this node over the window.
    /// Positive = net emissions, negative = net sequestration / avoidance.
    pub co2e_kg: Scalar,
    /// Node-level ecosafety scalar in [0,1], precomputed by ecosafety frames.
    /// 0 = no risk, 1 = maximal allowed corridor risk.
    pub ecosafety_risk: Scalar,
    /// Node-level benefit scalar in [0,1], e.g. pollutant mass removed fraction.
    pub eco_benefit: Scalar,
    /// Optional restoration radius in meters (local effect radius).
    /// If not supplied, it can be inferred from hydraulics or coverage density.
    pub restoration_radius_m: Option<Scalar>,
    /// Optional free-form metadata for RAG / dashboards.
    pub meta: HashMap<String, String>,
}

/// Ecoper-joule and related derived recognition fields.
/// All of these are non-actuating and intended for frames/orchestration only.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CyboquaticEcoPlot {
    /// Node identity.
    pub node_id: String,
    /// Region key.
    pub region: String,
    /// Ecological benefit per joule: eco_benefit / energy_j.
    /// 0 if energy_j <= 0.
    pub ecoper_joule: Scalar,
    /// CO₂e per joule: co2e_kg / energy_j.
    /// 0 if energy_j <= 0.
    pub co2e_per_joule: Scalar,
    /// Flag set true if node is net carbon-negative over the window.
    pub carbon_negative: bool,
    /// Node-level ecosafety risk mirrored from input.
    pub ecosafety_risk: Scalar,
    /// Node-level benefit mirrored from input.
    pub eco_benefit: Scalar,
    /// Restoration radius (meters).
    pub restoration_radius_m: Scalar,
}

/// A simple restoration surface that can be used by
/// v_cyboquatic_window_with_planes and higher-level frames.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CyboquaticRestorationSurface {
    /// Nodes contributing to this surface.
    pub nodes: Vec<CyboquaticEcoPlot>,
    /// Aggregated eco benefit across nodes (simple sum).
    pub total_eco_benefit: Scalar,
    /// Aggregated ecosafety risk across nodes (mean).
    pub mean_ecosafety_risk: Scalar,
    /// Aggregated CO₂e per joule across nodes (mean, weighted by energy).
    pub mean_co2e_per_joule: Scalar,
    /// Fraction of nodes that are carbon-negative.
    pub carbon_negative_fraction: Scalar,
}

/// Window descriptor used for frames/orchestration only.
/// Planes are logical slices in risk/benefit space (no actuation).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CyboquaticWindowPlane {
    /// Plane name, e.g. "high-benefit-low-risk".
    pub name: String,
    /// Inclusive lower bound on eco_benefit.
    pub min_benefit: Scalar,
    /// Inclusive upper bound on ecosafety_risk.
    pub max_risk: Scalar,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CyboquaticWindowWithPlanes {
    pub planes: Vec<CyboquaticWindowPlane>,
}

/// Primary index struct wiring samples, plots, and surfaces together.
/// This is explicitly non-actuating and is safe to use from any orchestration
/// layer that respects ALN corridors.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CyboquaticIndex {
    pub samples: Vec<CyboquaticNodeSample>,
    pub plots: Vec<CyboquaticEcoPlot>,
    pub restoration_surface: CyboquaticRestorationSurface,
}

/// Compute a single CyboquaticEcoPlot from a node sample.
pub fn compute_ecoplot(sample: &CyboquaticNodeSample) -> CyboquaticEcoPlot {
    let energy_j = sample.energy_j.max(0.0);
    let ecoper_joule = if energy_j > 0.0 {
        sample.eco_benefit / energy_j
    } else {
        0.0
    };
    let co2e_per_joule = if energy_j > 0.0 {
        sample.co2e_kg / energy_j
    } else {
        0.0
    };
    let carbon_negative = sample.co2e_kg < 0.0;
    let restoration_radius_m = sample.restoration_radius_m.unwrap_or(0.0);

    CyboquaticEcoPlot {
        node_id: sample.node_id.clone(),
        region: sample.region.clone(),
        ecoper_joule,
        co2e_per_joule,
        carbon_negative,
        ecosafety_risk: sample.ecosafety_risk,
        eco_benefit: sample.eco_benefit,
        restoration_radius_m,
    }
}

/// Build a CyboquaticRestorationSurface from a list of plots.
/// All aggregations are non-actuating and suitable for visualization frames.
pub fn build_restoration_surface(plots: &[CyboquaticEcoPlot]) -> CyboquaticRestorationSurface {
    if plots.is_empty() {
        return CyboquaticRestorationSurface {
            nodes: Vec::new(),
            total_eco_benefit: 0.0,
            mean_ecosafety_risk: 0.0,
            mean_co2e_per_joule: 0.0,
            carbon_negative_fraction: 0.0,
        };
    }

    let mut total_eco_benefit = 0.0;
    let mut sum_risk = 0.0;
    let mut sum_weighted_co2e = 0.0;
    let mut sum_weight = 0.0;
    let mut carbon_negative_count = 0usize;

    for plot in plots {
        total_eco_benefit += plot.eco_benefit;
        sum_risk += plot.ecosafety_risk;
        // Use |eco_benefit| as a simple proxy weight here; can be swapped
        // for actual energy or mass kernel in a later kernel crate.
        let weight = plot.eco_benefit.abs().max(1e-9);
        sum_weighted_co2e += plot.co2e_per_joule * weight;
        sum_weight += weight;
        if plot.carbon_negative {
            carbon_negative_count += 1;
        }
    }

    let n = plots.len() as Scalar;
    let mean_ecosafety_risk = sum_risk / n;
    let mean_co2e_per_joule = if sum_weight > 0.0 {
        sum_weighted_co2e / sum_weight
    } else {
        0.0
    };
    let carbon_negative_fraction = carbon_negative_count as Scalar / n;

    CyboquaticRestorationSurface {
        nodes: plots.to_vec(),
        total_eco_benefit,
        mean_ecosafety_risk,
        mean_co2e_per_joule,
        carbon_negative_fraction,
    }
}

/// Build a complete CyboquaticIndex from raw samples.
///
/// This function is suitable for being called by non-actuating frames.
/// Any downstream controller must still pass through ALN safesteprule and
/// deploydecisionkernel gates.
pub fn build_cyboquatic_index(samples: Vec<CyboquaticNodeSample>) -> CyboquaticIndex {
    let plots: Vec<CyboquaticEcoPlot> = samples.iter().map(compute_ecoplot).collect();
    let restoration_surface = build_restoration_surface(&plots);
    CyboquaticIndex {
        samples,
        plots,
        restoration_surface,
    }
}

/// Type used for region aggregation output in GeoJSON emitters.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RegionAggregate {
    pub region: String,
    pub node_count: u32,
    pub total_eco_benefit: Scalar,
    pub mean_ecosafety_risk: Scalar,
    pub carbon_negative_fraction: Scalar,
}

/// Group nodes by region and compute aggregated benefit / risk and
/// ecosafety distributions (here: simple mean risk and carbon-negative fraction).
pub fn aggregate_by_region(index: &CyboquaticIndex) -> Vec<RegionAggregate> {
    let mut by_region: HashMap<String, Vec<&CyboquaticEcoPlot>> = HashMap::new();
    for plot in &index.plots {
        by_region
            .entry(plot.region.clone())
            .or_insert_with(Vec::new)
            .push(plot);
    }

    let mut out = Vec::new();
    for (region, plots) in by_region {
        let node_count = plots.len() as u32;
        if node_count == 0 {
            continue;
        }
        let mut total_eco_benefit = 0.0;
        let mut sum_risk = 0.0;
        let mut carbon_negative_count = 0u32;

        for p in plots {
            total_eco_benefit += p.eco_benefit;
            sum_risk += p.ecosafety_risk;
            if p.carbon_negative {
                carbon_negative_count += 1;
            }
        }

        let mean_ecosafety_risk = sum_risk / node_count as Scalar;
        let carbon_negative_fraction = carbon_negative_count as Scalar / node_count as Scalar;

        out.push(RegionAggregate {
            region,
            node_count,
            total_eco_benefit,
            mean_ecosafety_risk,
            carbon_negative_fraction,
        });
    }

    out
}

/// Minimal GeoJSON feature representation for region heatmaps.
///
/// This struct is meant to be serialized directly to JSON strings that
/// CyboquaticOverlay.kt can consume as a feature collection.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GeoJsonFeature {
    #[serde(rename = "type")]
    pub feature_type: String,
    pub properties: serde_json::Value,
    pub geometry: serde_json::Value,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GeoJsonFeatureCollection {
    #[serde(rename = "type")]
    pub collection_type: String,
    pub features: Vec<GeoJsonFeature>,
}

/// Helper to emit a GeoJSON FeatureCollection from region aggregates and an
/// externally-supplied GeoJSON geometry map keyed by region.
///
/// `region_geometries` is expected to map region IDs to valid GeoJSON geometry
/// objects (e.g., Polygon or MultiPolygon). This function never invents geometry.
pub fn emit_region_geojson(
    aggregates: &[RegionAggregate],
    region_geometries: &HashMap<String, serde_json::Value>,
) -> GeoJsonFeatureCollection {
    let mut features = Vec::new();

    for agg in aggregates {
        if let Some(geometry) = region_geometries.get(&agg.region) {
            let props = serde_json::json!({
                "region": agg.region,
                "node_count": agg.node_count,
                "total_eco_benefit": agg.total_eco_benefit,
                "mean_ecosafety_risk": agg.mean_ecosafety_risk,
                "carbon_negative_fraction": agg.carbon_negative_fraction,
                // Convenience heatmap scalars for CyboquaticOverlay.kt:
                "heat_ecobenefit": agg.total_eco_benefit,
                "heat_risk": agg.mean_ecosafety_risk,
                "heat_carbon_negative": agg.carbon_negative_fraction
            });

            features.push(GeoJsonFeature {
                feature_type: "Feature".to_string(),
                properties: props,
                geometry: geometry.clone(),
            });
        }
    }

    GeoJsonFeatureCollection {
        collection_type: "FeatureCollection".to_string(),
        features,
    }
}

/// Sample structure for Phoenix synthetic risk samples.
/// This is aligned with ESPD and ecosafety frames and is non-actuating.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NodeRiskSample {
    pub node_id: String,
    pub region: String,
    /// Normalized PFAS / PFBS concentration risk [0,1].
    pub r_pfas: Scalar,
    /// Normalized E. coli / pathogen risk [0,1].
    pub r_pathogen: Scalar,
    /// Normalized salinity / TDS risk [0,1].
    pub r_salinity: Scalar,
    /// Normalized temperature risk [0,1].
    pub r_temperature: Scalar,
    /// Combined ecosafety scalar [0,1].
    pub ecosafety_risk: Scalar,
}

/// Simple ESPD-style aggregation for Phoenix synthetic scenarios.
/// This function only computes recognition risk and can be reused by
/// examples that emit ALN or CSV artifacts.
pub fn espd_ecosafety_from_sample(sample: &NodeRiskSample) -> Scalar {
    // Weighted combination of normalized risks; weights can be tuned via ALN.
    let w_pfas = 0.35;
    let w_pathogen = 0.35;
    let w_salinity = 0.20;
    let w_temperature = 0.10;

    let r = w_pfas * sample.r_pfas
        + w_pathogen * sample.r_pathogen
        + w_salinity * sample.r_salinity
        + w_temperature * sample.r_temperature;

    r.clamp(0.0, 1.0)
}

/// Generate a synthetic Phoenix NodeRiskSample given basic water-quality
/// statistics normalized in [0,1]. This is intended for examples only.
pub fn make_phoenix_synthetic_sample(
    node_id: &str,
    region: &str,
    r_pfas: Scalar,
    r_pathogen: Scalar,
    r_salinity: Scalar,
    r_temperature: Scalar,
) -> NodeRiskSample {
    let mut sample = NodeRiskSample {
        node_id: node_id.to_string(),
        region: region.to_string(),
        r_pfas,
        r_pathogen,
        r_salinity,
        r_temperature,
        ecosafety_risk: 0.0,
    };
    sample.ecosafety_risk = espd_ecosafety_from_sample(&sample);
    sample
}
