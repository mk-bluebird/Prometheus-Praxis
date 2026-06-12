// File: crates/econet_ker/src/lib.rs
// Repo target: github.com/mk-bluebird/eco_restoration_shard

//! EcoNet KER core types and validation logic.
//!
//! This crate encodes the safety-first, role-aware KER framework used to
//! validate EcoNet energy cells and Cyboquatic modules before deployment.
//!
//! All parameters are designed to align with the TOML specs under `policy/`
//! and `specs/` in the mono-repo.

#![forbid(unsafe_code)]
#![deny(warnings)]
#![allow(clippy::upper_case_acronyms)]

pub mod env;
pub mod roles;
pub mod cell;
pub mod module;
pub mod policy;
