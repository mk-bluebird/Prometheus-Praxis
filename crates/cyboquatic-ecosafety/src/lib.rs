// Filename: crates/cyboquatic-ecosafety/src/lib.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

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

// Corridor linkage and ALN binding.
// The ALN specification is embedded so that Rust types and grammar
// co-evolve with the authoritative protocol.

/// Embedded ALN specification for the ecosafety envelope.
///
/// This string must match the contents of
/// `qpudatashards/policies/CyboquaticEcosafetyEnvelopePhoenix2026v1.aln`
/// in the Prometheus-Praxis repository.
pub const ECOSAFETY_ALN_SPEC: &str =
    include_str!("../../qpudatashards/policies/CyboquaticEcosafetyEnvelopePhoenix2026v1.aln");

pub mod config;
pub mod ker;

// Core frame and pipeline primitives.
pub mod frame;
pub mod window;
pub mod lyapunov_regime;
pub mod risk;
pub mod covariance;
pub mod integrity;

// Schema, shard, and governance wiring.
pub mod aln_schema;
pub mod shard_schema;
pub mod shard_update_validator;
pub mod provenance;
pub mod provenancedetail;
pub mod provenancerecord;
pub mod provenanceexport;
pub mod governance_checker;

// Domain-specific diagnostic frames.
pub mod ecosafetycovarianceframe;
pub mod biodiversity_mesocosm;
pub mod pipeline3;
pub mod types;

/// Common configuration types for ecosafety frames.
pub use config::EcosafetyConfig;

/// Dynamic KER calculator based on covariance condition number and ecosafety distance.
pub use ker::KerFactors;

// Core diagnostic traits and context.
pub use frame::{CompositeFrame, Frame, FrameContext, FrameError};

// Windowing and status history.
pub use window::{
    EcosafetyStatus,
    EcosafetyStatusHistory,
    EcosafetyTrend,
    WindowManager,
};

// Lyapunov regime diagnostics.
pub use lyapunov_regime::{
    LyapunovStabilityDiagnostics,
    LyapunovStabilityFrame,
    VtHistory,
};

// Risk-space primitives and KER window representation.
pub use risk::{
    KERWindow,
    LyapunovResidual,
    LyapunovWeights,
    RiskCoord,
    RiskVector,
};

// Covariance-based ecosafety frame.
pub use covariance::{
    CovarianceOutput,
    CovarianceSample,
    EcosafetyCovarianceConfig,
    EcosafetyCovarianceFrame as CoreCovarianceFrame,
    LyapunovDistance,
};

// Integrity frame for adversarial or malformed inputs.
pub use integrity::{IntegrityCheckFrame, IntegrityDiagnostics};

// ALN-bound schema and shard update validation.
pub use aln_schema::{
    parse_ecosafety_envelope_schema,
    validate_update as validate_shard_update,
    ShardField,
    ShardFieldKind,
    ShardSchema as AlnShardSchema,
    ShardUpdate,
    ShardValidationError,
};

// SQL/ALN shard schema model and structural validator.
pub use shard_schema::ShardSchema;
pub use shard_update_validator::validate_update;

// Provenance tracking primitives and records.
pub use provenance::{Provenance, ProvenanceStep};
pub use provenancedetail::ProvenanceDetail;
pub use provenancerecord::EcosafetyProvenanceRecord;
pub use provenanceexport::{
    pipeline_output_to_provenance_records,
    provenance_record_to_csv_row,
};

// Governance checker that tags shard updates with sovereignty/consent hints.
pub use governance_checker::{GovernanceChecker, GovernanceTag};

// High-level three-stage pipeline (Integrity → Covariance → Biodiversity) with provenance.
pub use pipeline3::{
    buildecosafetypipeline3,
    EcosafetyPipeline3,
    EcosafetyPipelineOutput,
};

// Schema-bound ecosystem types mirroring ALN SQL records.
pub use types::{CyboNodeEcosafetyEnvelope, NodeRiskSample};

// Biodiversity mesocosm diagnostics.
pub use biodiversity_mesocosm::{
    BiodiversityIntegrityDiagnostics,
    BiodiversityIntegrityFrame,
    MesocosmRiskFrame,
    MesocosmShardRow,
};
