// filename: eco_restoration_shard_config/src/validate.rs

use std::fs::File;
use std::io::Read;
use std::path::{Path, PathBuf};

use serde::Serialize;
use thiserror::Error;

use crate::{Config, ProviderConfig, OWNER_DID_CANONICAL, DEFAULT_CONFIG_PATH};

/// Structured description of a single validation issue.
#[derive(Debug, Clone, Serialize)]
pub struct ConfigValidationIssue {
    /// Path-like identifier for the field, e.g. "owner_did" or "providers[0].endpoint".
    pub field: String,

    /// Short, stable code for programmatic handling, e.g. "MISSING_FIELD".
    pub code: String,

    /// Human-readable explanation of the issue.
    pub message: String,
}

/// Aggregated validation report for a config.
///
/// This type is used both in error returns and as a CI/AI-facing JSON payload.
#[derive(Debug, Clone, Serialize)]
pub struct ConfigValidationReport {
    /// Indicates whether the issues are fatal for normal operation.
    pub fatal: bool,

    /// Collection of individual issues discovered during validation.
    pub issues: Vec<ConfigValidationIssue>,
}

/// Error type for configuration load and validation.
#[derive(Debug, Error)]
pub enum ConfigError {
    /// Config file not found at the given path.
    #[error("config file not found at {0}")]
    NotFound(String),

    /// IO error while reading the file.
    #[error("failed to read config file {path}: {source}")]
    Io {
        path: String,
        #[source]
        source: std::io::Error,
    },

    /// YAML parse error while decoding into the Config struct.
    #[error("YAML parse error at {path}: {source}")]
    Parse {
        path: String,
        #[source]
        source: serde_yaml::Error,
    },

    /// Config was syntactically valid, but semantic validation failed.
    #[error("validation failed")]
    Validation(ConfigValidationReport),
}

/// Load `config.yaml` from the current working directory and validate it.
///
/// This is the most common entry point for binaries in the repo.
pub fn load_config() -> Result<Config, ConfigError> {
    let path = PathBuf::from(DEFAULT_CONFIG_PATH);
    load_config_from_path(&path)
}

/// Load a config from a specific path and validate it.
///
/// Callers that need custom locations should prefer this function.
pub fn load_config_from_path(path: &Path) -> Result<Config, ConfigError> {
    let path_str = path.to_string_lossy().to_string();

    if !path.exists() {
        return Err(ConfigError::NotFound(path_str));
    }

    let mut file = File::open(path).map_err(|e| ConfigError::Io {
        path: path_str.clone(),
        source: e,
    })?;

    let mut buf = String::new();
    file.read_to_string(&mut buf).map_err(|e| ConfigError::Io {
        path: path_str.clone(),
        source: e,
    })?;

    let config: Config = serde_yaml::from_str(&buf).map_err(|e| ConfigError::Parse {
        path: path_str,
        source: e,
    })?;

    validate(&config)
}

/// Validate a parsed Config, returning a cloned Config on success.
///
/// On failure, returns a `ConfigError::Validation` with a structured report.
pub fn validate(config: &Config) -> Result<Config, ConfigError> {
    let mut issues: Vec<ConfigValidationIssue> = Vec::new();

    // Owner DID must match the canonical DID for this ecosystem.
    if config.owner_did.trim().is_empty() {
        issues.push(ConfigValidationIssue {
            field: "owner_did".to_string(),
            code: "MISSING_FIELD".to_string(),
            message: "owner_did must be set and non-empty".to_string(),
        });
    } else if config.owner_did.trim() != OWNER_DID_CANONICAL {
        issues.push(ConfigValidationIssue {
            field: "owner_did".to_string(),
            code: "OWNER_DID_MISMATCH".to_string(),
            message: format!(
                "owner_did must equal canonical DID {canonical}, found {actual}",
                canonical = OWNER_DID_CANONICAL,
                actual = config.owner_did.trim(),
            ),
        });
    }

    // Version should be non-empty and follow a simple pattern.
    if config.version.trim().is_empty() {
        issues.push(ConfigValidationIssue {
            field: "version".to_string(),
            code: "MISSING_FIELD".to_string(),
            message: "version must be set and non-empty (e.g., 2026.v1)".to_string(),
        });
    }

    // At least one provider must be configured.
    if config.providers.is_empty() {
        issues.push(ConfigValidationIssue {
            field: "providers".to_string(),
            code: "EMPTY_LIST".to_string(),
            message: "providers list must contain at least one entry".to_string(),
        });
    }

    // Per-provider validation.
    for (idx, provider) in config.providers.iter().enumerate() {
        validate_provider(provider, idx, &mut issues);
    }

    if issues.is_empty() {
        Ok(config.clone())
    } else {
        let report = ConfigValidationReport {
            fatal: true,
            issues,
        };
        Err(ConfigError::Validation(report))
    }
}

fn validate_provider(provider: &ProviderConfig, index: usize, issues: &mut Vec<ConfigValidationIssue>) {
    let base = format!("providers[{index}]");

    if provider.name.trim().is_empty() {
        issues.push(ConfigValidationIssue {
            field: format!("{base}.name"),
            code: "MISSING_FIELD".to_string(),
            message: "provider.name must be set and non-empty".to_string(),
        });
    }

    if provider.endpoint.trim().is_empty() {
        issues.push(ConfigValidationIssue {
            field: format!("{base}.endpoint"),
            code: "MISSING_FIELD".to_string(),
            message: "provider.endpoint must be set and non-empty".to_string(),
        });
    }

    if provider.apikey_env.trim().is_empty() {
        issues.push(ConfigValidationIssue {
            field: format!("{base}.apikey_env"),
            code: "MISSING_FIELD".to_string(),
            message: "provider.apikey_env must be set and non-empty".to_string(),
        });
    }

    if provider.lane.trim().is_empty() {
        issues.push(ConfigValidationIssue {
            field: format!("{base}.lane"),
            code: "MISSING_FIELD".to_string(),
            message: "provider.lane must be set and non-empty".to_string(),
        });
    }

    // Enforce non-actuating providers in this repo.
    if !provider.non_actuating {
        issues.push(ConfigValidationIssue {
            field: format!("{base}.non_actuating"),
            code: "NON_ACTUATING_FALSE".to_string(),
            message: "provider.non_actuating must be true in eco_restoration_shard".to_string(),
        });
    }
}
