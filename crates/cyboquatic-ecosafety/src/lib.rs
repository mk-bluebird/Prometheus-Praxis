// Filename: crates/cyboquatic-ecosafety/src/lib.rs
#![deny(missing_docs)]
//! Cyboquatic ecosafety core for Phoenix-class nodes.
//!
//! This crate provides a KER-aware, non-actuating ecosafety kernel for
//! Cyboquatic nodes, aligned with the 2026 rx/Vt/KER grammar used across
//! Phoenix MAR basins, FOG routing workloads, and biodegradable substrates.
//!
//! # Ecosafety invariants
//!
//! - All risk planes are normalized to [0, 1] as `RiskCoord` values,
//!   with corridor-based normalization defined in ALN specs.
//! - The Lyapunov residual V_t = sum_j w_j r_j^2 is used as the
//!   scalar ecosafety channel, and control is only admissible when
//!   the residual is non-increasing outside a small safe interior.
//! - KER windows track knowledge-factor `K`, eco-impact `E`, and
//!   risk-of-harm `R` over rolling horizons, with deployability gates
//!   enforced via `ker_deployable()`.
//!
//! # Corridor linkage
//!
//! This crate is intended to be wired directly to ALN specifications
//! such as `CyboquaticEcosafetyEnvelopePhoenix2026v1`, which define
//! the corridor bands and schema columns for PFAS, CEC, SAT, surcharge,
//! biodiversity, Vt, lane, KER, evidencehex, and Bostrom DIDs.
//!
//! The narrative specification is included verbatim here to keep the
//! Rust types and the ALN grammar co-evolving.
#![doc = include_str!("../specs/CyboquaticEcosafetyEnvelopePhoenix2026v1.aln")]

mod risk;
mod node;
mod frame;
mod covariance;
mod integrity;

pub mod aln_schema;
pub mod window;
pub mod biodiversity_mesocosm;
pub mod lyapunov_regime;

pub use crate::covariance::{
    CovarianceOutput, CovarianceSample, EcosafetyCovarianceConfig, EcosafetyCovarianceFrame,
    LyapunovDistance,
};
pub use crate::frame::{CompositeFrame, Frame, FrameContext, FrameError};
pub use crate::integrity::{IntegrityCheckFrame, IntegrityDiagnostics};
pub use crate::node::{CyboLane, CyboNodeEcosafetyEnvelope, NodeRiskSample};
pub use crate::risk::{
    KERWindow, LyapunovResidual, LyapunovWeights, RiskCoord, RiskVector,
};

pub use aln_schema::{
    parse_ecosafety_envelope_schema, validate_update, ShardField, ShardFieldKind, ShardSchema,
    ShardUpdate, ShardValidationError,
};
pub use biodiversity_mesocosm::{
    BiodiversityIntegrityDiagnostics, BiodiversityIntegrityFrame, MesocosmRiskFrame,
    MesocosmShardRow,
};
pub use lyapunov_regime::{LyapunovStabilityDiagnostics, LyapunovStabilityFrame, VtHistory};
pub use window::{
    EcosafetyStatus, EcosafetyStatusHistory, EcosafetyTrend, WindowManager,
};
