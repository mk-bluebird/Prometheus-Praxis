// filename: eco_restoration_shard_config/src/model.rs

use serde::Deserialize;

/// Provider-level configuration for upstream services.
///
/// All providers must be non-actuating in this repo.
/// Endpoints are treated as opaque strings and never contacted by this crate.
#[derive(Debug, Clone, Deserialize)]
pub struct ProviderConfig {
    /// Logical name for the provider, used in logs and metrics.
    pub name: String,

    /// Base URL or address for the provider (never contacted by this crate).
    pub endpoint: String,

    /// Environment variable name that holds the API key or token for this provider.
    ///
    /// The actual key is never read here; this is a declarative binding only.
    pub apikey_env: String,

    /// Lane tag for the provider, e.g. "RESEARCH", "GOV", "OBSERVE".
    pub lane: String,

    /// Must be true for all providers in this repository.
    ///
    /// This is enforced in validation to keep the crate non-actuating.
    pub non_actuating: bool,
}

/// Top-level configuration for eco_restoration_shard.
///
/// This struct is the canonical parse target for `config.yaml`.
#[derive(Debug, Clone, Deserialize)]
pub struct Config {
    /// Config schema version tag, e.g. "2026.v1".
    pub version: String,

    /// Owner DID that governs this configuration.
    ///
    /// Must match the canonical Bostrom DID for this repo.
    pub owner_did: String,

    /// List of upstream providers that higher-level crates may route through.
    pub providers: Vec<ProviderConfig>,
}
