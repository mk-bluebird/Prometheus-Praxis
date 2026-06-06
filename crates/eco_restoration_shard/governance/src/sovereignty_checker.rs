// filename: governance/src/sovereignty_checker.rs
// destination: eco_restoration_shard/governance/src/sovereignty_checker.rs

use std::path::Path;

use anyhow::Result;

/// Minimal view of the AbsoluteDataSovereigntyPolicy2026v1 shard.
#[derive(Debug, Clone)]
pub struct AbsoluteDataSovereigntyPolicy {
    pub policy_id: String,
    pub owner_did: String,
    pub host_did: String,
    pub protected_extensions: Vec<String>,
    pub binding_event_kinds: Vec<String>,
    pub requires_contribution: bool,
    pub contribution_target: String,
}

/// Minimal view of the CyberCoreMigrationAuthority2026v1 shard.
#[derive(Debug, Clone)]
pub struct CyberCoreMigrationAuthority {
    pub clause_id: String,
    pub owner_did: String,
    pub host_did: String,
    pub required_files_glob: String,
    pub forbidden_substitutions: Vec<String>,
    pub require_clause_presence: bool,
    pub require_literal_ownerdid: bool,
    pub require_literal_hostdid: bool,
    pub lock_on_violation: bool,
    pub notify_director_on_violation: bool,
}

/// CI context for a single run.
#[derive(Debug, Clone)]
pub struct CiRunContext {
    /// Unique identifier for the CI run (e.g. GitHub run ID).
    pub run_id: String,
    /// Commit hash or change set identifier.
    pub change_id: String,
    /// Human-readable workflow name.
    pub workflow_name: String,
}

/// A single file diff entry in a CI run.
#[derive(Debug, Clone)]
pub struct DiffFile {
    /// Repository-relative path, using '/' separators.
    pub path: String,
    /// Optional previous contents (None for added files).
    pub old_contents: Option<String>,
    /// Optional new contents (None for deleted files).
    pub new_contents: Option<String>,
}

/// Result of sovereignty and migration checks for a CI run.
#[derive(Debug, Clone)]
pub struct SovereigntyCheckResult {
    /// Whether all checks passed without violations.
    pub ok: bool,
    /// Human-readable summary for CI logs.
    pub summary: String,
    /// Detailed per-file messages, useful for diagnostics.
    pub details: Vec<String>,
    /// Whether CI should hard-fail this run.
    pub should_fail_pipeline: bool,
    /// Whether terminals / sessions associated with this run should be locked.
    pub should_lock_sessions: bool,
}

/// Non-actuating interface for enforcing data sovereignty and migration authority in CI.
///
/// Implementations must be pure and side-effect free with respect to external systems:
/// they operate only on in-memory diffs and loaded governance shards.
pub trait SovereigntyChecker {
    /// Return the active absolute data sovereignty policy.
    fn sovereignty_policy(&self) -> &AbsoluteDataSovereigntyPolicy;

    /// Return the active migration authority clause.
    fn migration_authority(&self) -> &CyberCoreMigrationAuthority;

    /// Repository root path (used only for diagnostics and path normalization).
    fn repo_root(&self) -> &Path;

    /// Check data sovereignty constraints against the provided file diffs.
    ///
    /// Responsibilities:
    /// - Identify any file whose extension is in `protected_extensions`.
    /// - Require that any such file change is accompanied by a valid contribution
    ///   entry in the ContributionLedger2026v1 shard for this CI run.
    /// - Detect obvious policy avoidance, such as extension stripping or
    ///   moving protected content into unprotected paths.
    fn check_data_sovereignty(
        &self,
        ctx: &CiRunContext,
        diffs: &[DiffFile],
    ) -> Result<SovereigntyCheckResult>;

    /// Check migration authority invariants against the provided file diffs.
    ///
    /// Responsibilities:
    /// - Enforce presence of the migration clause in required files.
    /// - Prevent removal or substitution of `owner_did` / `host_did` literals.
    /// - Detect attempts to replace the migration authority with forbidden patterns.
    fn check_migration_authority(
        &self,
        ctx: &CiRunContext,
        diffs: &[DiffFile],
    ) -> Result<SovereigntyCheckResult>;

    /// Combined check helper that enforces both policies for a CI run.
    ///
    /// Implementations should:
    /// - Invoke `check_data_sovereignty` and `check_migration_authority`.
    /// - Merge results conservatively (any violation propagates to failure).
    fn check_all(
        &self,
        ctx: &CiRunContext,
        diffs: &[DiffFile],
    ) -> Result<SovereigntyCheckResult>;
}
