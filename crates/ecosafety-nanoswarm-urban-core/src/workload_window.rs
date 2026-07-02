// Filename: crates/ecosafety-nanoswarm-urban-core/src/workload_window.rs
// rust-version = "1.85", edition = "2024"
// License: MIT OR Apache-2.0

use serde::{Deserialize, Serialize};

use crate::types::{KerTriplet, ShardRowBase};

/// Mirrors your existing workloadledger / workload window summary fields,
/// but anchored to a specific node/asset and time window.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct WorkloadNodeWindow {
    /// Shard base: shardid, timestamputc (window close), objectid, KER.
    pub base: ShardRowBase,

    /// Node identifier (logical node, pump, softrobot, etc.).
    pub node_id: String,

    /// Optional asset identifier for Cyboquatic machinery.
    pub asset_id: Option<String>,

    /// Window start and end timestamps (UTC, seconds since UNIX epoch).
    pub window_start_utc: i64,
    pub window_end_utc: i64,

    /// Energy request and surplus in Joules, aligned with workloadledger.
    pub energy_req_j: f64,
    pub energy_surplus_j: f64,

    /// Accepted, rejected, and rerouted fractions (0..1) of workload.
    pub accepted_fraction: f64,
    pub rejected_fraction: f64,
    pub rerouted_fraction: f64,

    /// Mean Lyapunov residual before and after the window.
    pub mean_vt_before: f64,
    pub mean_vt_after: f64,
    pub mean_delta_vt: f64,

    /// Mean carbon and biodiversity risk scalars (0..1), optional.
    pub mean_r_carbon: Option<f64>,
    pub mean_r_biodiv: Option<f64>,

    /// Corridor status and decision mode, mirroring ALN enums.
    /// e.g., "SATISFIED", "SOFTVIOLATION", "HARDVIOLATION".
    pub corridor_status: String,
    /// e.g., "ALLOW", "DERATE", "STOP".
    pub decision_mode: String,

    /// Cached KER triplet at window level (can equal base.ker or be derived).
    pub window_ker: KerTriplet,
}

impl WorkloadNodeWindow {
    /// Builder-style constructor that enforces minimal invariants and
    /// mirrors your ALN workload schema.
    #[allow(clippy::too_many_arguments)]
    pub fn build(
        // Shard identity and object binding.
        shardid: String,
        window_close_utc: i64,
        objectid: String,
        base_ker: KerTriplet,

        // Node/asset scope.
        node_id: String,
        asset_id: Option<String>,

        // Window bounds.
        window_start_utc: i64,
        window_end_utc: i64,

        // Energy and workload fractions.
        energy_req_j: f64,
        energy_surplus_j: f64,
        accepted_fraction: f64,
        rejected_fraction: f64,
        rerouted_fraction: f64,

        // Lyapunov residual traces.
        mean_vt_before: f64,
        mean_vt_after: f64,

        // Risk scalars.
        mean_r_carbon: Option<f64>,
        mean_r_biodiv: Option<f64>,

        // Corridor semantics.
        corridor_status: String,
        decision_mode: String,
    ) -> Self {
        let mean_delta_vt = mean_vt_after - mean_vt_before;

        // Build base shard row.
        let base = ShardRowBase {
            shardid,
            timestamputc: window_close_utc,
            objectid,
            ker: base_ker,
        };

        // Normalize fractions defensively to keep them in [0,1] and avoid NaN.
        fn clamp01(x: f64) -> f64 {
            if !x.is_finite() {
                0.0
            } else if x < 0.0 {
                0.0
            } else if x > 1.0 {
                1.0
            } else {
                x
            }
        }

        let accepted_fraction = clamp01(accepted_fraction);
        let rejected_fraction = clamp01(rejected_fraction);
        let rerouted_fraction = clamp01(rerouted_fraction);

        // Derive a window-level KER hint from Lyapunov and risk traces.
        // This aligns with your knowledgeecoscore and shardinstance patterns.
        let window_ker = KerTriplet {
            // Knowledge: high when window is well-formed and vt_after <= vt_before.
            k: if mean_vt_after <= mean_vt_before { 0.9 } else { 0.7 },
            // Eco-impact: high when accepted_fraction is high and risks are low.
            e: {
                let acc = accepted_fraction;
                let rc = mean_r_carbon.unwrap_or(0.0);
                let rb = mean_r_biodiv.unwrap_or(0.0);
                let risk_penalty = (rc + rb) / 2.0;
                (acc * (1.0 - risk_penalty)).clamp(0.0, 1.0)
            },
            // Risk-of-harm: proportional to corridor severity and positive delta Vt.
            r: {
                let base_r = if corridor_status == "HARDVIOLATION" {
                    0.9
                } else if corridor_status == "SOFTVIOLATION" {
                    0.4
                } else {
                    0.1
                };
                let vt_penalty = if mean_delta_vt > 0.0 {
                    (mean_delta_vt / (1.0 + mean_vt_before.abs)).clamp(0.0, 0.9)
                } else {
                    0.0
                };
                (base_r + vt_penalty).clamp(0.0, 1.0)
            },
        };

        WorkloadNodeWindow {
            base,
            node_id,
            asset_id,
            window_start_utc,
            window_end_utc,
            energy_req_j,
            energy_surplus_j,
            accepted_fraction,
            rejected_fraction,
            rerouted_fraction,
            mean_vt_before,
            mean_vt_after,
            mean_delta_vt,
            mean_r_carbon,
            mean_r_biodiv,
            corridor_status,
            decision_mode,
            window_ker,
        }
    }
}
