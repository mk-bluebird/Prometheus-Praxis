// eco_restoration_shard/cybercore/crates/consent-kernel/src/lib.rs

#![forbid(unsafe_code)]
#![deny(warnings)]

use ed25519_dalek::{Signature, Verifier, VerifyingKey};
use serde::{Deserialize, Serialize};

/// Canonical consent state for eco-data routing from a sovereign host.
///
/// Intentionally small and closed; any additional modes must be
/// implemented as orthogonal flags, not new states, to keep proofs tight.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[repr(u8)]
pub enum ConsentState {
    /// No active consent; default at host onboarding.
    Dormant = 0,

    /// Explicit consent for eco-only data sharing is active.
    /// Other domains (clinical, psych) must use distinct consents.
    OptedInEcoOnly = 1,

    /// Consent is temporarily suspended by a psych-risk floor or gate.
    /// No new eco-routing may be activated while in this state.
    SuspendedByPsychrisk = 2,

    /// Consent is fully revoked. This is an absorbing state unless a new,
    /// cryptographically fresh consent envelope is presented.
    Revoked = 3,
}

/// Bostrom-bound consent envelope for eco-only data use.
///
/// This is the only object allowed to *activate* or *reactivate* sharing.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ConsentEnvelope {
    /// Canonical host DID (e.g., did.aln.organic-host).
    pub host_did: String,

    /// Primary Bostrom address that owns this consent.
    pub bostrom_address: String,

    /// Domain scoping — must equal "eco_only" for this kernel.
    pub scope: String,

    /// Monotone version or nonce; strictly increasing per host.
    pub version: u64,

    /// UNIX timestamp (seconds) at which consent expires.
    pub expires_at_unix: u64,

    /// Ed25519 signature by the primary Bostrom key
    /// over the canonical envelope body.
    pub signature: Signature,
}

/// Public key registry entry for a Bostrom address.
#[derive(Clone, Debug)]
pub struct BostromKeyBinding {
    pub bostrom_address: String,
    pub verifying_key: VerifyingKey,
}

/// Errors for consent transitions.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ConsentError {
    /// Envelope is missing required eco-only scope.
    WrongScope,
    /// Envelope is signed by a key that does not match the primary Bostrom address.
    BadSignature,
    /// Envelope has expired.
    Expired,
    /// Envelope version is not strictly greater than the last accepted version.
    NonMonotoneVersion,
    /// Attempted illegal state transition (e.g., reactivation without envelope).
    InvalidTransition,
}

/// Canonical, signature-stable encoding of the envelope body.
///
/// NOTE: This deliberately excludes `signature` so that the same
/// bytes are signed and verified across implementations.
fn envelope_signing_bytes(env: &ConsentEnvelope) -> Vec<u8> {
    let mut out = Vec::new();
    out.extend(env.host_did.as_bytes());
    out.push(0x1f);
    out.extend(env.bostrom_address.as_bytes());
    out.push(0x1f);
    out.extend(env.scope.as_bytes());
    out.push(0x1f);
    out.extend(env.version.to_be_bytes());
    out.push(0x1f);
    out.extend(env.expires_at_unix.to_be_bytes());
    out
}

/// Verify that the envelope is:
/// - eco-only in scope,
/// - signed by the primary Bostrom key,
/// - not expired,
/// - strictly version-increasing.
pub fn verify_consent_envelope(
    env: &ConsentEnvelope,
    key_binding: &BostromKeyBinding,
    now_unix: u64,
    last_version: Option<u64>,
) -> Result<(), ConsentError> {
    if env.scope.as_str() != "eco_only" {
        return Err(ConsentError::WrongScope);
    }

    if env.bostrom_address != key_binding.bostrom_address {
        return Err(ConsentError::BadSignature);
    }

    if env.expires_at_unix <= now_unix {
        return Err(ConsentError::Expired);
    }

    if let Some(prev) = last_version {
        if env.version <= prev {
            return Err(ConsentError::NonMonotoneVersion);
        }
    }

    let msg = envelope_signing_bytes(env);
    key_binding
        .verifying_key
        .verify(&msg, &env.signature)
        .map_err(|_| ConsentError::BadSignature)
}

/// Pure transition function for the consent FSM.
///
/// This is the function to subject to Kani proofs. All eco-shards
/// must route consent changes through this kernel.
pub fn next_consent_state(
    current: ConsentState,
    env: Option<&ConsentEnvelope>,
    env_ok: bool,
) -> Result<ConsentState, ConsentError> {
    match (current, env, env_ok) {
        // Dormant -> OptedInEcoOnly requires a valid envelope.
        (ConsentState::Dormant, Some(_), true) => Ok(ConsentState::OptedInEcoOnly),

        // OptedInEcoOnly -> SuspendedByPsychrisk is driven by psych-risk gates, no envelope.
        (ConsentState::OptedInEcoOnly, None, _) => Ok(ConsentState::SuspendedByPsychrisk),

        // SuspendedByPsychrisk -> OptedInEcoOnly requires a still-valid envelope.
        (ConsentState::SuspendedByPsychrisk, Some(_), true) => Ok(ConsentState::OptedInEcoOnly),

        // Any non-Revoked state -> Revoked can occur without an envelope e.g., host revocation,
        // emergency neuro-rights gate, or legal order.
        (ConsentState::Dormant, None, _)
        | (ConsentState::OptedInEcoOnly, None, _)
        | (ConsentState::SuspendedByPsychrisk, None, _) => Ok(ConsentState::Revoked),

        // Revoked -> Revoked with no envelope (absorbing w.r.t. eco-shards).
        (ConsentState::Revoked, None, _) => Ok(ConsentState::Revoked),

        // Revoked + invalid envelope => stay revoked.
        (ConsentState::Revoked, Some(_), false) => Ok(ConsentState::Revoked),

        // Revoked + valid envelope => re-activate eco-only sharing.
        (ConsentState::Revoked, Some(_), true) => Ok(ConsentState::OptedInEcoOnly),

        // Any other combination is illegal.
        _ => Err(ConsentError::InvalidTransition),
    }
}
