//! Governance checker for ecosafety shard updates.
//!
//! This module embeds absolute data-sovereignty policy SQL as static
//! strings and provides a `GovernanceChecker` that tags ecosafety
//! envelopes with governance-related hints. It remains fully
//! non-actuating: it does not open database connections or perform
//! IO. Callers are responsible for executing any SQL with read-only
//! semantics and feeding the results into this checker.

#![forbid(unsafe_code)]

use crate::types::CyboNodeEcosafetyEnvelope;

/// High-level governance tag indicating required consent or review.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum GovernanceTag {
    /// Envelope is consistent with current sovereignty policy.
    SovereigntyOk,
    /// Envelope touches a sensitive site and requires host consent.
    HostConsentRequired,
    /// Envelope touches a sensitive site and requires community FPIC.
    CommunityFpicRequired,
    /// Envelope should be reviewed by governance before lane promotion.
    GovernanceReviewRequired,
}

/// Result of applying governance checks to an envelope.
#[derive(Clone, Debug)]
pub struct GovernanceCheckResult {
    envelope: CyboNodeEcosafetyEnvelope,
    tags: Vec<GovernanceTag>,
}

impl GovernanceCheckResult {
    /// Access the underlying envelope.
    pub fn envelope(&self) -> &CyboNodeEcosafetyEnvelope {
        &self.envelope
    }

    /// Governance tags produced for this envelope.
    pub fn tags(&self) -> &[GovernanceTag] {
        &self.tags
    }

    /// Returns true if the envelope is considered governance-clean.
    pub fn is_ok(&self) -> bool {
        self.tags.iter().all(|t| matches!(t, GovernanceTag::SovereigntyOk))
    }
}

/// Governance checker configuration.
///
/// This struct holds policy thresholds that influence how tags are
/// derived. It can be populated from read-only SQL queries over
/// `db_absolute_data_sovereignty_policy.sql` and related governance
/// shards by an external caller.
#[derive(Clone, Debug)]
pub struct GovernanceChecker {
    /// Whether the underlying site is flagged as sensitive.
    pub issensitive: bool,
    /// Lane classification for this node (e.g., "EXP", "PROD").
    pub lane: String,
    /// Whether the node is tied to BCI or neurorights-sensitive assets.
    pub neurorights_sensitive: bool,
}

impl GovernanceChecker {
    /// Construct a new governance checker for a given node lane and sensitivity.
    pub fn new(issensitive: bool, lane: impl Into<String>, neurorights_sensitive: bool) -> Self {
        Self {
            issensitive,
            lane: lane.into(),
            neurorights_sensitive,
        }
    }

    /// Apply governance mapping logic to an ecosafety envelope.
    ///
    /// Mapping (non-exhaustive, but monotone and conservative):
    /// - If `issensitive` and `neurorights_sensitive`, tag both
    ///   `HostConsentRequired` and `CommunityFpicRequired`.
    /// - If `issensitive` only, tag `CommunityFpicRequired`.
    /// - If lane is `PROD`, always tag `GovernanceReviewRequired`.
    /// - Otherwise, tag `SovereigntyOk`.
    pub fn apply(
        &self,
        envelope: CyboNodeEcosafetyEnvelope,
    ) -> GovernanceCheckResult {
        let mut tags = Vec::new();

        if self.issensitive && self.neurorights_sensitive {
            tags.push(GovernanceTag::HostConsentRequired);
            tags.push(GovernanceTag::CommunityFpicRequired);
        } else if self.issensitive {
            tags.push(GovernanceTag::CommunityFpicRequired);
        }

        if self.lane.eq_ignore_ascii_case("PROD") {
            tags.push(GovernanceTag::GovernanceReviewRequired);
        }

        if tags.is_empty() {
            tags.push(GovernanceTag::SovereigntyOk);
        }

        GovernanceCheckResult { envelope, tags }
    }
}

/// Embedded absolute data-sovereignty policy SQL for reference.
///
/// Callers should execute this SQL (or the migration that created it)
/// in read-only mode and build `GovernanceChecker` instances based on
/// the resulting policy rows. Keeping this string here ensures that
/// the Rust governance logic and the SQL policies cannot silently
/// diverge.
pub const DB_ABSOLUTE_DATA_SOVEREIGNTY_POLICY_SQL: &str = include_str!(
    "../../governance/db_absolute_data_sovereignty_policy.sql"
);

/// Embedded contribution ledger SQL for cross-checking DIDs and consent.
pub const DB_CONTRIBUTION_LEDGER_SQL: &str = include_str!(
    "../../governance/db_contribution_ledger.sql"
);
