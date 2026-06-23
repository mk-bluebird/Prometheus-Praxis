use serde::{Deserialize, Serialize};
use time::{OffsetDateTime, format_description::well_known::Rfc3339};
use uuid::Uuid;

use crate::model::CityCapability;

/// Newtype for binding id — prevents cross-host reassignment being treated as a trivial integer.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq, Hash)]
pub struct BindingId(pub i64);

/// Immutable binding of a capability to a specific host.
/// Design (D): Non-transferable by construction (sealed flag).
/// NR: 0 — No neurological linkage; identity is DID + Bostrom.
/// EE: Host-local remaining_taps field avoids online counters.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CityPassBinding {
    pub binding_id: BindingId,
    /// Host envelope DID — root identity tuple anchor.
    pub host_did: String,
    /// Capability id (UUID) this binding wraps.
    pub capability_id: Uuid,
    /// Jurisdictional policy shortcut.
    pub jurisdiction_policy: String,
    /// Issuance time in UTC.
    pub issued_at_utc: String,
    /// Remaining taps on this host.
    pub remaining_taps: u32,
    /// Once sealed, this binding can never be reassigned.
    pub sealed: bool,
}

impl CityPassBinding {
    pub fn is_exhausted(&self) -> bool {
        self.remaining_taps == 0
    }
}

/// Errors for binding.
///
/// Any attempt to rebind a sealed capability is rejected; this enforces non-transferability.
#[derive(Debug, thiserror::Error)]
pub enum BindingError {
    #[error("binding already sealed for capability {capability_id}")]
    AlreadySealed { capability_id: Uuid },
    #[error("remaining taps exhausted")]
    NoRemainingTaps,
}

/// In-memory binding registry entry for issuance checks.
#[derive(Debug, Clone)]
pub struct BindingRegistryEntry {
    pub binding: CityPassBinding,
}

pub struct BindingRegistry {
    entries: Vec<BindingRegistryEntry>,
}

impl BindingRegistry {
    pub fn new() -> Self {
        Self { entries: Vec::new() }
    }

    pub fn get_by_capability(&self, cap_id: Uuid) -> Option<&BindingRegistryEntry> {
        self.entries.iter().find(|e| e.binding.capability_id == cap_id)
    }

    pub fn insert(&mut self, entry: BindingRegistryEntry) {
        self.entries.push(entry);
    }
}

/// Bind a CityCapability to a host, ensuring non-reassignment.
/// Design: Calling this twice with same capability but different host_did yields BindingError::AlreadySealed.
pub fn bind_city_pass(
    registry: &mut BindingRegistry,
    host_did: String,
    capability: &CityCapability,
    binding_id: BindingId,
    issued_at: OffsetDateTime,
) -> Result<CityPassBinding, BindingError> {
    if let Some(existing) = registry.get_by_capability(capability.cap_id) {
        if existing.binding.sealed {
            return Err(BindingError::AlreadySealed {
                capability_id: capability.cap_id,
            });
        }
    }

    let binding = CityPassBinding {
        binding_id,
        host_did,
        capability_id: capability.cap_id,
        jurisdiction_policy: capability.jurisdiction_policy.clone(),
        issued_at_utc: issued_at.format(&Rfc3339).unwrap(),
        remaining_taps: capability.max_taps,
        sealed: true,
    };

    registry.insert(BindingRegistryEntry {
        binding: binding.clone(),
    });

    Ok(binding)
}
