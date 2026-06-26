// Path: rust/identity/src/braindid_alias_resolver.rs
// Role: Local-first BrainDid alias resolver for Prometheus-Praxis.
// Edition: 2024, Rust 1.85

use std::collections::HashMap;
use std::sync::{Arc, RwLock};

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum DidScope {
    HealthTcr,
    Ecosafety,
    WaterErm,
    Cyboquatic,
    CityAnalytics,
    ResearchOnly,
    TestFixture,
}

impl DidScope {
    pub fn as_str(&self) -> &'static str {
        match self {
            DidScope::HealthTcr => "HEALTH_TCR",
            DidScope::Ecosafety => "ECOSAFETY",
            DidScope::WaterErm => "WATER_ERM",
            DidScope::Cyboquatic => "CYBOQUATIC",
            DidScope::CityAnalytics => "CITY_ANALYTICS",
            DidScope::ResearchOnly => "RESEARCH_ONLY",
            DidScope::TestFixture => "TEST_FIXTURE",
        }
    }
}

#[derive(Debug, Clone)]
pub struct PrimaryBrainDid {
    pub primary_did: String,
}

#[derive(Debug, Clone)]
pub struct BrainDidAlias {
    pub alias_did: String,
    pub primary_did: String,
    pub scope: DidScope,
    pub active: bool,
}

#[derive(Debug, thiserror::Error)]
pub enum AliasError {
    #[error("primary_did is empty")]
    EmptyPrimaryDid,
    #[error("alias_did is empty")]
    EmptyAliasDid,
    #[error("alias already exists for scope {scope}")]
    AliasAlreadyExists { scope: String },
    #[error("alias not found")]
    AliasNotFound,
    #[error("alias inactive")]
    AliasInactive,
}

/// In-memory alias registry for local-first agents.
///
/// In production, this should be backed by the ALN and SQLite schemas that
/// implement ALE-IDENTITY-BRAINDID-ALIASING.V1. This module deliberately
/// exposes only alias resolution and registration, with no access to
/// identity-classification or rights logic.
#[derive(Debug, Default, Clone)]
pub struct BrainDidAliasRegistry {
    inner: Arc<RwLock<HashMap<DidScope, BrainDidAlias>>>,
}

impl BrainDidAliasRegistry {
    pub fn new() -> Self {
        Self {
            inner: Arc::new(RwLock::new(HashMap::new())),
        }
    }

    /// Register a new alias for a scope. Fails if an active alias already exists.
    pub fn register_alias(
        &self,
        primary: &PrimaryBrainDid,
        alias_did: String,
        scope: DidScope,
    ) -> Result<BrainDidAlias, AliasError> {
        if primary.primary_did.trim().is_empty() {
            return Err(AliasError::EmptyPrimaryDid);
        }
        if alias_did.trim().is_empty() {
            return Err(AliasError::EmptyAliasDid);
        }

        let mut map = self
            .inner
            .write()
            .expect("BrainDidAliasRegistry lock poisoned");

        if let Some(existing) = map.get(&scope) {
            if existing.active {
                return Err(AliasError::AliasAlreadyExists {
                    scope: scope.as_str().to_string(),
                });
            }
        }

        let alias = BrainDidAlias {
            alias_did,
            primary_did: primary.primary_did.clone(),
            scope: scope.clone(),
            active: true,
        };

        map.insert(scope, alias.clone());
        Ok(alias)
    }

    /// Mark an alias for a scope as inactive.
    pub fn revoke_alias(&self, scope: &DidScope) -> Result<(), AliasError> {
        let mut map = self
            .inner
            .write()
            .expect("BrainDidAliasRegistry lock poisoned");

        let existing = map.get_mut(scope).ok_or(AliasError::AliasNotFound)?;
        existing.active = false;
        Ok(())
    }

    /// Resolve the active alias for a scope, if any.
    pub fn resolve_alias(&self, scope: &DidScope) -> Result<BrainDidAlias, AliasError> {
        let map = self
            .inner
            .read()
            .expect("BrainDidAliasRegistry lock poisoned");

        let alias = map.get(scope).cloned().ok_or(AliasError::AliasNotFound)?;
        if !alias.active {
            return Err(AliasError::AliasInactive);
        }
        Ok(alias)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn register_and_resolve_alias_per_scope() {
        let registry = BrainDidAliasRegistry::new();
        let primary = PrimaryBrainDid {
            primary_did: "did:ppx:brain:primary-001".to_string(),
        };

        let alias = registry
            .register_alias(
                &primary,
                "did:ppx:alias:health-001".to_string(),
                DidScope::HealthTcr,
            )
            .expect("alias registration should succeed");

        assert_eq!(alias.primary_did, primary.primary_did);
        assert_eq!(alias.scope, DidScope::HealthTcr);

        let resolved = registry
            .resolve_alias(&DidScope::HealthTcr)
            .expect("alias should resolve");

        assert_eq!(resolved.alias_did, "did:ppx:alias:health-001");
    }

    #[test]
    fn cannot_register_second_active_alias_for_same_scope() {
        let registry = BrainDidAliasRegistry::new();
        let primary = PrimaryBrainDid {
            primary_did: "did:ppx:brain:primary-002".to_string(),
        };

        registry
            .register_alias(
                &primary,
                "did:ppx:alias:ecosafety-001".to_string(),
                DidScope::Ecosafety,
            )
            .expect("first alias registration should succeed");

        let err = registry
            .register_alias(
                &primary,
                "did:ppx:alias:ecosafety-002".to_string(),
                DidScope::Ecosafety,
            )
            .expect_err("second active alias should fail");

        match err {
            AliasError::AliasAlreadyExists { scope } => {
                assert_eq!(scope, DidScope::Ecosafety.as_str());
            }
            _ => panic!("unexpected error variant"),
        }
    }

    #[test]
    fn revoke_makes_alias_inactive() {
        let registry = BrainDidAliasRegistry::new();
        let primary = PrimaryBrainDid {
            primary_did: "did:ppx:brain:primary-003".to_string(),
        };

        registry
            .register_alias(
                &primary,
                "did:ppx:alias:test-001".to_string(),
                DidScope::TestFixture,
            )
            .expect("alias registration should succeed");

        registry
            .revoke_alias(&DidScope::TestFixture)
            .expect("revoke should succeed");

        let err = registry
            .resolve_alias(&DidScope::TestFixture)
            .expect_err("resolved alias should now be inactive");

        matches!(err, AliasError::AliasInactive | AliasError::AliasNotFound);
    }
}
