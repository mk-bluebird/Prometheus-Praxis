// File: crates/materials-eco-knowledge-labeller/src/lib.rs
#![deny(missing_docs)]
//! materials-eco-knowledge-labeller
//!
//! A small, sovereign crate for building and labelling an eco‑knowledge corpus
//! of textual material descriptions (e.g., from building permits) for use in
//! transformer fine‑tuning. It mirrors the Python pipeline discussed earlier:
//!  - ingest raw descriptions,
//!  - pre‑filter / prioritise candidates,
//!  - apply a rubric to compute an eco‑knowledge factor `k_material` in [0,1],
//!  - emit machine‑readable JSON suitable for training.
//!
//! This crate does **not** perform any model training; it focuses on data
//! preparation and label consistency, suitable for integration with
//! Prometheus‑Praxis KER semantics.

pub mod model;
pub mod heuristics;
pub mod rubric;
pub mod io;
