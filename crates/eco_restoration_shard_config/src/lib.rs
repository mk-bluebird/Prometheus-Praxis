// filename: eco_restoration_shard_config/src/lib.rs

//! eco_restoration_shard_config
//!
//! Non-actuating configuration loader for mk-bluebird/eco_restoration_shard.
//!
//! Design goals:
//! - Read-only: no outbound network, no actuation, filesystem reads only.
//! - DID-bound: config must be anchored to the canonical Bostrom DID.
//! - Machine-readable validation: structured error reports suitable for CI and AI tools.
//! - Stable contract: small, explicit surface for other crates to depend on.

mod model;
mod validate;

pub use crate::model::{Config, ProviderConfig};
pub use crate::validate::{
    load_config, load_config_from_path, ConfigError, ConfigValidationIssue, ConfigValidationReport,
};

/// Canonical Bostrom DID for this ecosystem.
///
/// This is the anchor used to prevent silent rehoming of governance.
pub const OWNER_DID_CANONICAL: &str = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";

/// Default relative path to the config file, used by `load_config`.
pub const DEFAULT_CONFIG_PATH: &str = "config.yaml";
