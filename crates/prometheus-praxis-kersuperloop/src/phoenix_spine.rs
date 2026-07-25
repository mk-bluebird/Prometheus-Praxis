// filename: crates/prometheus-praxis-kersuperloop/src/phoenix_spine.rs
// destination: https://github.com/mk-bluebird/Prometheus-Praxis/crates/prometheus-praxis-kersuperloop/src/phoenix_spine.rs
// edition: 2024
// rust-version = "1.85"
// license: MIT OR Apache-2.0

#![forbid(unsafe_code)]

use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use thiserror::Error;

use cyboquatic_blastradius_spine::{CyboSpine, SpineError, WorkloadNodeWindow};
use crate::{compute_always_improve, KerSummary, PhoenixKerWindowDiagnostic, PhoenixKerWindowInput, Scalar};

/// Error type for Phoenix spine wiring and CLI.
#[derive(Debug, Error)]
pub enum PhoenixSpineError {
    #[error("spine error: {0}")]
    Spine(#[from] SpineError),
    #[error("superloop error: {0}")]
    Superloop(#[from] crate::SuperloopError),
}

/// Enriched diagnostic tying Cyboquatic workload window to KER superloop.[file:14][file:13]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PhoenixNodeDiagnostic {
    pub node_id: String,
    pub region: String,
    pub window_start_utc: DateTime<Utc>,
    pub window_end_utc: DateTime<Utc>,
    pub total_req_j: Scalar,
    pub total_surplus_j: Scalar,
    pub mean_vt_before: Scalar,
    pub mean_vt_after: Scalar,
    pub mean_carbon_risk: Scalar,
    pub mean_biodiv_risk: Scalar,
    pub accepts: u64,
    pub rejects: u64,
    pub reroutes: u64,
    pub accept_fraction: Scalar,
    pub ker: KerSummary,
    pub always_improve: crate::AlwaysImproveScore,
    pub admissible: bool,
}

/// Convert a WorkloadNodeWindow (CyboSpine view over vcyboworkloadnodewindow)[file:14]
/// plus a precomputed KER triad into a PhoenixKerWindowInput.
fn window_to_input(
    window: &WorkloadNodeWindow,
    k: Scalar,
    e: Scalar,
    r: Scalar,
) -> PhoenixKerWindowInput {
    PhoenixKerWindowInput {
        node_id: window.nodeid.clone(),
        region: window.region.clone(),
        window_start_utc: window.windowstartutc,
        window_end_utc: window.windowendutc,
        total_req_j: window.totalreqj,
        total_surplus_j: window.totalsurplusj,
        mean_vt_before: window.meanvtbefore,
        mean_vt_after: window.meanvasfter,
        mean_carbon_risk: window.meanrcarbon.unwrap_or(0.0),
        mean_biodiv_risk: window.meanrbiodiv.unwrap_or(0.0),
        k,
        e,
        r,
    }
}

/// Evaluate KER-Lyapunov diagnostics for a Phoenix node by reading
/// vcyboworkloadnodewindow through CyboSpine and then calling the superloop.[file:14][file:13]
///
/// Inputs:
/// - db_path: path to the SQLite file containing Cyboquatic blastradius/workload schema.
/// - node_id: EcoNet / hydrology node identifier.
/// - region: region label, e.g. "Phoenix-AZ".
/// - k/e/r: KER triad for this node or shard, in [0,1].
///
/// Output:
/// - PhoenixNodeDiagnostic serialized to JSON by the CLI.
pub fn evaluate_phoenix_node_from_spine(
    db_path: &str,
    node_id: &str,
    region: &str,
    k: Scalar,
    e: Scalar,
    r: Scalar,
) -> Result<Option<PhoenixNodeDiagnostic>, PhoenixSpineError> {
    let spine = CyboSpine::opendb(db_path)?;
    let maybe_window = spine.summarizeworkloadnoderegion(node_id, region)?;

    let window = match maybe_window {
        Some(w) => w,
        None => return Ok(None),
    };

    // Build KER summary.
    let ker = KerSummary::new(k, e, r)?;

    // Always-improve score directly from window fields.[file:14]
    let ai = compute_always_improve(
        window.totalreqj,
        window.totalsurplusj,
        window.meanrcarbon.unwrap_or(0.0),
        window.meanrbiodiv.unwrap_or(0.0),
        window.meanvtbefore,
        window.meanvtafter,
    )?;

    let admissible = ai.carbon_ceiling_ok && ai.residual_nonpositive_ok;

    Ok(Some(PhoenixNodeDiagnostic {
        node_id: window.nodeid.clone(),
        region: window.region.clone(),
        window_start_utc: window.windowstartutc,
        window_end_utc: window.windowendutc,
        total_req_j: window.totalreqj,
        total_surplus_j: window.totalsurplusj,
        mean_vt_before: window.meanvtbefore,
        mean_vt_after: window.meanvasfter,
        mean_carbon_risk: window.meanrcarbon.unwrap_or(0.0),
        mean_biodiv_risk: window.meanrbiodiv.unwrap_or(0.0),
        accepts: window.accepts,
        rejects: window.rejects,
        reroutes: window.reroutes,
        accept_fraction: window.acceptfraction,
        ker,
        always_improve: ai,
        admissible,
    }))
}
