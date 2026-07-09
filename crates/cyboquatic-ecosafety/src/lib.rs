#![forbid(unsafe_code)]
#![deny(missing_docs)]
#![deny(clippy::unwrap_used)]
#![deny(clippy::expect_used)]
#![deny(clippy::panic)]
#![deny(clippy::todo)]
#![deny(clippy::unimplemented)]
#![deny(clippy::disallowed_methods)]

//! Cyboquatic ecosafety core for Phoenix-class nodes.
//!
//! This crate provides a KER-aware, non-actuating ecosafety kernel for
//! Cyboquatic nodes, aligned with the 2026 rx/Vt/KER grammar used across
//! Phoenix MAR basins, FOG routing workloads, and biodegradable substrates.
//!
//! - All risk planes are normalized to [0, 1] as `RiskCoord` values, with
//!   corridor-based normalization defined in ALN specifications.
//! - The Lyapunov residual V_t = sum_j w_j r_j^2 is used as the scalar
//!   ecosafety channel, and control is only admissible when the residual is
//!   non-increasing outside a small safe interior.
//! - KER windows track knowledge-factor `K`, eco-impact `E`, and risk-of-harm
//!   `R` over rolling horizons, with deployability gates enforced via
//!   `ker_deployable()`.
//!
//! This crate also provides privacy-preserving aggregation of ecosafety
//! statistics across multiple node operators using additive sharing and
//! optional differential privacy. All outputs are advisory and non-actuating.

/// SAFE_FLAG governance signal model for the iCE40 Lyapunov kernel.
pub mod safe_flag;
pub use safe_flag::{SafeFlagModel, SafeFlagState};

/// Privacy-preserving aggregation primitives for ecosafety risk.
pub mod privacy;

pub use crate::privacy::{
    AggregatedShares,
    DpConfig,
    DpGlobalRiskStats,
    GlobalRiskStats,
    LaplaceSampler,
    LocalRiskStats,
    RiskShare,
    apply_dp_to_global_stats,
    make_risk_shares,
    reconstruct_global_stats,
};

mod config;
mod ker;
mod frame;
mod window;
mod lyapunov_regime;
mod risk;
mod covariance;
mod integrity;
mod aln_schema;
mod shard_schema;
mod shard_update_validator;
mod provenance;
mod provenancedetail;
mod provenancerecord;
mod provenanceexport;
mod governance_checker;
mod ecosafetycovarianceframe;
mod biodiversity_mesocosm;
mod pipeline3;
mod types;
mod node;
mod fog_guard;

/// Embedded ALN specification for the ecosafety envelope.
///
/// This string must match the contents of
/// `specs/CyboquaticEcosafetyEnvelopePhoenix2026v1.aln`
/// in the Prometheus-Praxis repository.
pub const ECOSAFETY_ALN_SPEC: &str =
    include_str!("../specs/CyboquaticEcosafetyEnvelopePhoenix2026v1.aln");

/// Configuration types for ecosafety frames.
pub mod config_reexport {
    pub use crate::config::EcosafetyConfig;
}

/// Dynamic KER calculator based on covariance condition number and ecosafety distance.
pub use ker::KerFactors;

/// Core diagnostic traits and context.
pub use frame::{CompositeFrame, Frame, FrameContext, FrameError};

/// Windowing and status history types.
pub use window::{
    EcosafetyStatus,
    EcosafetyStatusHistory,
    EcosafetyTrend,
    WindowManager,
};

/// Lyapunov regime diagnostics.
pub use lyapunov_regime::{
    LyapunovStabilityDiagnostics,
    LyapunovStabilityFrame,
    VtHistory,
};

/// Risk-space primitives and KER window representation.
pub use risk::{
    KERWindow,
    LyapunovResidual,
    LyapunovWeights,
    RiskCoord,
    RiskVector,
};

/// Covariance-based ecosafety frame.
pub use covariance::{
    CovarianceOutput,
    CovarianceSample,
    EcosafetyCovarianceConfig,
    EcosafetyCovarianceFrame as CoreCovarianceFrame,
    LyapunovDistance,
};

/// Integrity frame for adversarial or malformed inputs.
pub use integrity::{IntegrityCheckFrame, IntegrityDiagnostics};

/// ALN-bound schema and shard update validation.
pub use aln_schema::{
    parse_ecosafety_envelope_schema,
    validate_update as validate_shard_update,
    ShardField,
    ShardFieldKind,
    ShardSchema as AlnShardSchema,
    ShardUpdate,
    ShardValidationError,
};

/// SQL/ALN shard schema model.
pub use shard_schema::ShardSchema;

/// Shard update validator for ecosafety shards.
pub use shard_update_validator::validate_update;

/// Provenance tracking primitives and records.
pub use provenance::{Provenance, ProvenanceStep};
pub use provenancedetail::ProvenanceDetail;
pub use provenancerecord::EcosafetyProvenanceRecord;
pub use provenanceexport::{
    pipeline_output_to_provenance_records,
    provenance_record_to_csv_row,
};

/// Governance checker that tags shard updates with sovereignty/consent hints.
pub use governance_checker::{GovernanceChecker, GovernanceTag};

/// High-level three-stage pipeline (Integrity → Covariance → Biodiversity) with provenance.
pub use pipeline3::{
    buildecosafetypipeline3,
    EcosafetyPipeline3,
    EcosafetyPipelineOutput,
};

/// Schema-bound ecosystem types mirroring ALN SQL records.
pub use types::{CyboNodeEcosafetyEnvelope, NodeRiskSample};

/// Biodiversity mesocosm diagnostics.
pub use biodiversity_mesocosm::{
    BiodiversityIntegrityDiagnostics,
    BiodiversityIntegrityFrame,
    MesocosmRiskFrame,
    MesocosmShardRow,
};

/// FOG guard primitives.
pub use fog_guard::{
    FogGuard,
    FogGuardBands,
    FogGuardConfig,
    FogGuardInput,
    FogGuardKerThresholds,
    FogGuardVerdict,
};

/// Construct a `FogGuardInput` from a `CyboNodeEcosafetyEnvelope` and an explicit
/// `corridor_present` flag.
///
/// This function is the canonical wiring between ecosafety envelopes and the
/// FOG guard; all routing and sewer actuation gates should pass through it.
pub fn fog_guard_input_from_envelope(
    envelope: &CyboNodeEcosafetyEnvelope,
    corridor_present: bool,
) -> FogGuardInput {
    let ker = envelope.ker();
    let risk = envelope.risk();
    let residual = envelope.residual();

    let k = RiskCoord::new_clamped(ker.k());
    let e = RiskCoord::new_clamped(ker.e());
    let r = RiskCoord::new_clamped(ker.r());

    FogGuardInput {
        risk,
        residual,
        corridor_present,
        safestep_ok: ker.kerdeployable(),
        k,
        e,
        r,
    }
}

/// Evaluate a safestep verdict for a `CyboNodeEcosafetyEnvelope`.
///
/// Callers supply:
/// - `envelope`: the current ecosafety state for the node,
/// - `corridor_present`: whether a valid corridor exists for this step,
/// - `cfg`: optional guard configuration; if `None`, defaults are used.
///
/// This helper is the single entry point that FOG routers and sewer planners
/// should call before proposing any actuation.
pub fn safestep(
    envelope: &CyboNodeEcosafetyEnvelope,
    corridor_present: bool,
    cfg: Option<FogGuardConfig>,
) -> FogGuardVerdict {
    let guard_cfg = cfg.unwrap_or_else(FogGuardConfig::default);
    let guard = FogGuard::new(guard_cfg);
    let input = fog_guard_input_from_envelope(envelope, corridor_present);
    guard.evaluate(&input)
}

/// Non-actuating smoke test for the `safestep` helper.
///
/// This function is intended for use in unit tests or diagnostics to verify
/// that the default guard configuration accepts a clearly safe envelope and
/// rejects one that violates KER and RoH constraints.
///
/// It does not touch any hardware or external IO.
pub fn safestep_smoke_test() {
    let risk = RiskVector {
        rcec: RiskCoord::new_clamped(0.0),
        rsat: RiskCoord::new_clamped(0.0),
        rsurcharge: RiskCoord::new_clamped(0.0),
        rbiodiv: RiskCoord::new_clamped(0.0),
        rvt: RiskCoord::new_clamped(0.0),
        rgovernance: RiskCoord::new_clamped(0.0),
    };

    let weights = LyapunovWeights::equal();
    let prev_residual = LyapunovResidual { value: 0.0 };
    let mut ker = KERWindow::new();
    ker.update(prev_residual, prev_residual, risk);

    let envelope = CyboNodeEcosafetyEnvelope::new(
        crate::types::CyboLane::Production,
        risk,
        weights,
        prev_residual,
        ker,
        "0x00".to_string(),
        "did:bostrom:test".to_string(),
    );

    let verdict_ok = safestep(&envelope, true, None);
    assert!(matches!(verdict_ok, FogGuardVerdict::Allow));

    let mut ker_bad = KERWindow::new();
    ker_bad.update(
        prev_residual,
        prev_residual,
        RiskVector {
            rcec: RiskCoord::new_clamped(0.9),
            rsat: RiskCoord::new_clamped(0.9),
            rsurcharge: RiskCoord::new_clamped(0.9),
            rbiodiv: RiskCoord::new_clamped(0.9),
            rvt: RiskCoord::new_clamped(0.9),
            rgovernance: RiskCoord::new_clamped(0.9),
        },
    );

    let bad_envelope = CyboNodeEcosafetyEnvelope::new(
        crate::types::CyboLane::Production,
        risk,
        weights,
        prev_residual,
        ker_bad,
        "0x00".to_string(),
        "did:bostrom:test".to_string(),
    );

    let verdict_bad = safestep(&bad_envelope, true, None);
    assert!(matches!(verdict_bad, FogGuardVerdict::Stop));
}
