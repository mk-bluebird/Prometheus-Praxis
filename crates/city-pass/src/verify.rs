use serde::{Deserialize, Serialize};
use time::OffsetDateTime;

use crate::{
    binding::CityPassBinding,
    eco::EcoImpact,
    model::CityCapability,
    revocation::RevocationStore,
};

/// Verification outcome stores updated binding and eco impact for persistence.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VerificationOutcome {
    pub binding: CityPassBinding,
    pub eco: EcoImpact,
}

#[derive(Debug, thiserror::Error)]
pub enum VerificationError {
    #[error("capability revoked")]
    Revoked,
    #[error("pass expired")]
    Expired,
    #[error("pass not yet valid")]
    NotYetValid,
    #[error("no remaining taps")]
    NoRemainingTaps,
    #[error("time error: {0}")]
    TimeError(String),
    #[error("revocation store error: {0}")]
    Store(String),
}

/// Offline tap verification logic.
/// Sequence:
/// 1. Check revocation (append-only).
/// 2. Check validity window.
/// 3. Check remaining taps.
/// 4. Decrement taps and update eco savings.
/// Design (D): Monotonic — no path to re-enable revoked or expired passes.
/// NR: 0 — purely logical.
/// EE: Local check; no network access.
pub fn verify_tap(
    rev_store: &RevocationStore,
    capability_hex: &str,
    capability: &CityCapability,
    mut binding: CityPassBinding,
    mut eco: EcoImpact,
    now: OffsetDateTime,
) -> Result<VerificationOutcome, VerificationError> {
    if rev_store
        .is_revoked(capability_hex)
        .map_err(|e| VerificationError::Store(e.to_string()))?
    {
        return Err(VerificationError::Revoked);
    }

    let (start, end) = capability
        .validity_window()
        .map_err(|e| VerificationError::TimeError(e.to_string()))?;

    if now < start {
        return Err(VerificationError::NotYetValid);
    }

    if now > end {
        return Err(VerificationError::Expired);
    }

    if binding.remaining_taps == 0 {
        return Err(VerificationError::NoRemainingTaps);
    }

    binding.remaining_taps -= 1;
    eco.add_tap_savings();

    Ok(VerificationOutcome { binding, eco })
}
