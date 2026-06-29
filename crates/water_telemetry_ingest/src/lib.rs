// Filename: crates/water_telemetry_ingest/src/lib.rs
// Destination: eco_restoration_shard/crates/water_telemetry_ingest/src/lib.rs
//
// Rust edition: 2024
// rust-version = "1.85"
// License: MIT OR Apache-2.0
//
// This crate ingests water.tank.telemetry.v1 particles emitted by embedded C controllers,
// computes Lyapunov health indices, and exposes them to the Phoenix ERM stack.

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

/// ALN particle for tank telemetry: water.tank.telemetry.v1
///
/// This is the canonical Rust representation of records coming from the embedded C loop.
/// Each record corresponds to one control cycle at an edge controller.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WaterTankTelemetryV1 {
    /// Logical node identifier, e.g. "phx.district.downtown.tankpair.01"
    pub node_id: String,
    /// UTC timestamp in seconds since Unix epoch.
    pub timestamp_utc_s: i64,
    /// Tank 1 level in meters.
    pub h1_m: f32,
    /// Tank 2 level in meters.
    pub h2_m: f32,
    /// Lyapunov scalar V(t) = P1 * h1_tilde^2 + P2 * h2_tilde^2.
    pub v_lyapunov: f32,
    /// Normalized pump command 0.0 .. 1.0.
    pub u_cmd_norm: f32,
}

/// Per-node Lyapunov health indices over a window of telemetry samples.
///
/// These scalars are intended to feed Phoenix ERM metrics and dashboards.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LyapunovHealthIndices {
    /// Node identifier (same as in telemetry).
    pub node_id: String,
    /// Number of samples used.
    pub sample_count: usize,
    /// Mean V over the window.
    pub v_mean: f64,
    /// Maximum V observed.
    pub v_max: f64,
    /// Fraction of consecutive steps where V(t+1) - V(t) > 0.
    pub frac_v_increasing: f64,
    /// Maximum positive delta V(t+1) - V(t).
    pub max_delta_v_pos: f64,
    /// Maximum negative delta V(t+1) - V(t).
    pub max_delta_v_neg: f64,
}

/// Compute Lyapunov health indices for a sequence of telemetry samples
/// belonging to a single node.
///
/// The input slice must contain samples for exactly one node_id; the function
/// uses the node_id from the first sample.
pub fn compute_lyapunov_health_indices(samples: &[WaterTankTelemetryV1]) -> Option<LyapunovHealthIndices> {
    if samples.is_empty() {
        return None;
    }

    let node_id = samples[0].node_id.clone();
    let sample_count = samples.len();

    let mut sum_v = 0.0_f64;
    let mut v_max = f32::NEG_INFINITY;

    for s in samples {
        let v = s.v_lyapunov as f64;
        sum_v += v;
        if s.v_lyapunov > v_max {
            v_max = s.v_lyapunov;
        }
    }

    let v_mean = sum_v / (sample_count as f64);

    let mut increasing_count: usize = 0;
    let mut max_delta_v_pos: f64 = 0.0;
    let mut max_delta_v_neg: f64 = 0.0;

    for window in samples.windows(2) {
        let v0 = window[0].v_lyapunov as f64;
        let v1 = window[1].v_lyapunov as f64;
        let delta = v1 - v0;
        if delta > 0.0 {
            increasing_count += 1;
            if delta > max_delta_v_pos {
                max_delta_v_pos = delta;
            }
        } else {
            let neg_delta = delta;
            if neg_delta < max_delta_v_neg {
                max_delta_v_neg = neg_delta;
            }
        }
    }

    let frac_v_increasing = if sample_count > 1 {
        increasing_count as f64 / ((sample_count - 1) as f64)
    } else {
        0.0
    };

    Some(LyapunovHealthIndices {
        node_id,
        sample_count,
        v_mean,
        v_max: v_max as f64,
        frac_v_increasing,
        max_delta_v_pos,
        max_delta_v_neg,
    })
}

/// Aggregate indices across many nodes to a simple district-level scalar.
///
/// This demonstrates a minimal aggregation; in the Phoenix ERM stack this can be
/// expanded into richer metrics.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DistrictLyapunovSummary {
    pub district_id: String,
    /// Number of nodes contributing.
    pub node_count: usize,
    /// Mean of v_mean across nodes.
    pub v_mean_mean: f64,
    /// Max of v_max across nodes.
    pub v_max_max: f64,
    /// Mean of frac_v_increasing across nodes.
    pub frac_v_increasing_mean: f64,
}

pub fn aggregate_district_summary(
    district_id: &str,
    node_indices: &[LyapunovHealthIndices],
) -> Option<DistrictLyapunovSummary> {
    if node_indices.is_empty() {
        return None;
    }

    let mut sum_v_mean = 0.0;
    let mut sum_frac_incr = 0.0;
    let mut v_max_max = f64::NEG_INFINITY;

    for idx in node_indices {
        sum_v_mean += idx.v_mean;
        sum_frac_incr += idx.frac_v_increasing;
        if idx.v_max > v_max_max {
            v_max_max = idx.v_max;
        }
    }

    let node_count = node_indices.len();
    let v_mean_mean = sum_v_mean / (node_count as f64);
    let frac_v_increasing_mean = sum_frac_incr / (node_count as f64);

    Some(DistrictLyapunovSummary {
        district_id: district_id.to_string(),
        node_count,
        v_mean_mean,
        v_max_max,
        frac_v_increasing_mean,
    })
}
