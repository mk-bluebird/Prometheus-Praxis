// file: crates/eco-restoration-governance/src/proposed_fix.rs
#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

/// Monotone K/E/R impact projection for a ProposedFix, aligned with
/// existing Responsibility/Fairness geometry.
/// K = Knowledge, E = Ecological responsibility r-axis, R = Risk/RoH.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct KerImpactProjection {
    /// Expected delta on the knowledge axis (e.g., model quality, corridor resolution).
    pub delta_k: f64,
    /// Expected delta on the ecological responsibility axis r (must be >= 0.0 for admission).
    pub delta_e: f64,
    /// Expected delta on risk / RoH; forward-only corridors require this to be <= 0.0
    /// or bounded by the configured RoH ceiling in the validator.
    pub delta_r: f64,
}

/// ProposedFix particles queued in governance_review_queue before promotion
/// into the main evolution window. This struct is designed to be monotone,
/// host-sovereign, and compatible with existing manifest/evidence wiring.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProposedFix {
    /// Stable identifier for this fix, unique within the shard.
    pub fix_id: String,

    /// Crate name proposing the fix, e.g., "kerresidual", "lane-governance", "blastradius".
    pub source_crate: String,

    /// Fully-qualified topology target for this fix, such as a lane, sensor, or guard node
    /// (for example: "lane:restoration.t07", "sensor:watershed/alpha", or
    /// "guard:daily-evolution-loop-core").
    pub target_topology_item: String,

    /// Serialized patch or diff in base64, referencing the exact crate/branch version.
    /// This is treated as opaque by governance_review_queue and is decoded only in
    /// CI and evolution windows with matching git commits and manifest hashes.
    pub suggested_diff_base64: String,

    /// Monotone K/E/R impact projection, evaluated by prefairness validators and
    /// K/E/R guards before any promotion into a ResearchManifest.
    pub ker_impact_projection: KerImpactProjection,

    /// Ordered list of DIDs that must sign this ProposedFix before it can be
    /// promoted out of governance_review_queue. The validator enforces:
    /// - host_did must be present,
    /// - mk-bluebird / Cybercore authority DIDs must be included for host-critical lanes,
    /// - any additional ALN-governed roles (EthicsBoard, Steward, etc.) can be encoded.
    pub required_signing_dids: Vec<String>,
}
