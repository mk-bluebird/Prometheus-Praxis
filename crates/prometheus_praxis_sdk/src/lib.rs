// File: crates/prometheus_praxis_sdk/src/lib.rs
// License: MIT OR Apache-2.0
// Edition: 2024
// rust-version = "1.85"
//
// Unified governance SDK slice for Prometheus-Praxis.
// This crate provides the frozen grammar for telemetry,
// KER coordinates, Lyapunov residuals, lane configuration,
// governance gates, and lane decisions.
//
// All logic is non-actuating: it evaluates telemetry and
// produces decisions and evidence. Actuation must be
// implemented in separate, explicitly-governed stacks.

#![forbid(unsafe_code)]

pub mod lanes;
pub mod ffi;
pub mod kani_harnesses;
