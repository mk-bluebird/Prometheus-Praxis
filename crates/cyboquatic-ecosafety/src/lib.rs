// Filename: crates/cyboquatic-ecosafety/src/lib.rs
#![deny(missing_docs)]
//! Cyboquatic ecosafety core for Phoenix-class nodes.
//!
//! This crate provides a KER-aware, non-actuating ecosafety kernel for
//! Cyboquatic nodes, aligned with the 2026 rx/Vt/KER grammar used across
//! Phoenix MAR basins, FOG routing workloads, and biodegradable substrates.[file:24]
//!
//! # Ecosafety invariants
//!
//! - All risk planes are normalized to \[0, 1\] as `RiskCoord` values,
//!   with corridor-based normalization defined in ALN specs.[file:24]
//! - The Lyapunov residual \(V_t = \sum_j w_j r_j^2\) is used as the
//!   scalar ecosafety channel, and control is only admissible when
//!   the residual is non-increasing outside a small safe interior.[file:24]
//! - KER windows track knowledge-factor `K`, eco-impact `E`, and
//!   risk-of-harm `R` over rolling horizons, with deployability gates
//!   enforced via `ker_deployable()`.[file:23][file:24]
//!
//! # Corridor linkage
//!
//! This crate is intended to be wired directly to ALN specifications
//! such as `CyboquaticEcosafetyEnvelopePhoenix2026v1`, which define
//! the corridor bands and schema columns for PFAS, CEC, SAT, surcharge,
//! biodiversity, Vt, lane, KER, evidencehex, and Bostrom DIDs.[file:23][file:25]
//!
//! The narrative specification is included verbatim here to keep the
//! Rust types and the ALN grammar co-evolving:
#![doc = include_str!("../specs/CyboquaticEcosafetyEnvelopePhoenix2026v1.aln")]

mod risk;
mod node;
mod frame;

pub use crate::risk::{
    KERWindow, LyapunovResidual, LyapunovWeights, RiskCoord, RiskVector,
};
pub use crate::node::{CyboLane, CyboNodeEcosafetyEnvelope, NodeRiskSample};
pub use crate::frame::{CompositeFrame, Frame, FrameContext, FrameError};
