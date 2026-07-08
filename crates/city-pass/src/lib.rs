// crates/city-pass/src/lib.rs

#![forbid(unsafe_code)]
#![deny(warnings)]

//! Sovereign Phoenix CityPass core crate
//! Design (D): Low risk (local-only, explicit invariants)
//! Neuro-Risk (NR): Very low (no biophysical inputs or actuation)
//! Energy-Efficiency (EE): High (offline verification, CPU-only)

pub mod model;
pub mod binding;
pub mod revocation;
pub mod verify;
pub mod eco;
pub mod hexcommit;

pub use crate::binding::{bind_city_pass, BindingId, CityPassBinding};
pub use crate::eco::{EcoConfig, EcoImpact};
pub use crate::hexcommit::HexCommit;
pub use crate::model::{
    CityCapability, CityCapabilityDomain, CityPass, Holder, PassId, SignatureAlgo, SignedPass,
    ValidityWindow,
};
pub use crate::revocation::{RevocationReason, RevocationRecord, RevocationStore};
pub use crate::verify::{verify_tap, KeyError, SignError, VerificationError, VerificationOutcome};

use ed25519_dalek::{Signer, Verifier};
use rand::rngs::OsRng;
use serde::{Deserialize, Serialize};
use thiserror::Error;
use time::{OffsetDateTime, PrimitiveDateTime};
use uuid::Uuid;

/// Opaque identifier for a city pass, UUID v4 under the hood.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct PassId(Uuid);

impl PassId {
    pub fn new() -> Self {
        Self(Uuid::new_v4())
    }

    pub fn parse_str(s: &str) -> Result<Self, IdError> {
        let uuid = Uuid::parse_str(s).map_err(IdError::InvalidUuid)?;
        Ok(Self(uuid))
    }

    pub fn as_uuid(&self) -> Uuid {
        self.0
    }

    pub fn to_string(&self) -> String {
        self.0.to_string()
    }
}

/// Minimal user-facing identity for a pass holder.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Holder {
    pub pseudonym: String,
    pub region: String,
}

impl Holder {
    pub fn new<S: Into<String>, R: Into<String>>(pseudonym: S, region: R) -> Self {
        Self {
            pseudonym: pseudonym.into(),
            region: region.into(),
        }
    }
}

/// Temporal validity window for a pass.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct ValidityWindow {
    pub start: OffsetDateTime,
    pub end: OffsetDateTime,
}

impl ValidityWindow {
    pub fn new(start: OffsetDateTime, end: OffsetDateTime) -> Self {
        Self { start, end }
    }

    pub fn contains(&self, t: OffsetDateTime) -> bool {
        t >= self.start && t < self.end
    }

    pub fn is_well_formed(&self) -> bool {
        self.start < self.end
    }
}

/// Core transit pass data (unsigned).
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct CityPass {
    pub id: PassId,
    pub holder: Holder,
    pub validity: ValidityWindow,
    pub lane: String,
}

impl CityPass {
    pub fn new(id: PassId, holder: Holder, validity: ValidityWindow, lane: String) -> Self {
        Self {
            id,
            holder,
            validity,
            lane,
        }
    }

    pub fn is_currently_valid(&self, now: OffsetDateTime) -> bool {
        self.validity.is_well_formed() && self.validity.contains(now)
    }
}

/// Ed25519 key pair for signing passes.
#[derive(Debug)]
pub struct SigningKey {
    inner: ed25519_dalek::SigningKey,
}

impl SigningKey {
    pub fn generate() -> Self {
        let mut rng = OsRng;
        let inner = ed25519_dalek::SigningKey::generate(&mut rng);
        Self { inner }
    }

    pub fn from_bytes(bytes: &[u8; 32]) -> Self {
        let inner = ed25519_dalek::SigningKey::from_bytes(bytes);
        Self { inner }
    }

    pub fn verifying_key(&self) -> VerifyingKey {
        VerifyingKey {
            inner: self.inner.verifying_key(),
        }
    }

    pub fn sign_pass(&self, pass: &CityPass) -> Result<SignedPass, SignError> {
        let payload = serialize_pass_for_signing(pass)?;
        let signature = self.inner.sign(&payload);
        Ok(SignedPass {
            pass: pass.clone(),
            signature: signature.to_bytes().to_vec(),
            algo: SignatureAlgo::Ed25519DalekV1,
        })
    }
}

/// Ed25519 verifying key wrapper.
#[derive(Debug, Clone)]
pub struct VerifyingKey {
    inner: ed25519_dalek::VerifyingKey,
}

impl VerifyingKey {
    pub fn from_bytes(bytes: &[u8; 32]) -> Result<Self, KeyError> {
        let inner =
            ed25519_dalek::VerifyingKey::from_bytes(bytes).map_err(KeyError::InvalidVerifyingKey)?;
        Ok(Self { inner })
    }

    pub fn to_bytes(&self) -> [u8; 32] {
        self.inner.to_bytes()
    }

    pub fn verify_pass(&self, signed: &SignedPass) -> Result<(), VerifyError> {
        if signed.algo != SignatureAlgo::Ed25519DalekV1 {
            return Err(VerifyError::UnsupportedAlgorithm);
        }
        let payload = serialize_pass_for_signing(&signed.pass)?;
        let sig = ed25519_dalek::Signature::from_slice(&signed.signature)
            .map_err(VerifyError::InvalidSignatureBytes)?;
        self.inner
            .verify(&payload, &sig)
            .map_err(|_| VerifyError::SignatureMismatch)
    }
}

/// Algorithm marker for signatures.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SignatureAlgo {
    Ed25519DalekV1,
}

/// Signed pass: payload + detached signature + algorithm label.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct SignedPass {
    pub pass: CityPass,
    pub signature: Vec<u8>,
    pub algo: SignatureAlgo,
}

fn serialize_pass_for_signing(pass: &CityPass) -> Result<Vec<u8>, SignError> {
    let json = serde_json::to_vec(pass).map_err(SignError::Serialization)?;
    Ok(json)
}

#[derive(Debug, Error)]
pub enum IdError {
    #[error("invalid UUID: {0}")]
    InvalidUuid(uuid::Error),
}

#[derive(Debug, Error)]
pub enum SignError {
    #[error("serialization error: {0}")]
    Serialization(#[from] serde_json::Error),
}

#[derive(Debug, Error)]
pub enum VerifyError {
    #[error("unsupported signature algorithm")]
    UnsupportedAlgorithm,
    #[error("invalid signature bytes: {0}")]
    InvalidSignatureBytes(ed25519_dalek::SignatureError),
    #[error("signature mismatch")]
    SignatureMismatch,
    #[error("serialization error: {0}")]
    Serialization(#[from] serde_json::Error),
}

#[derive(Debug, Error)]
pub enum KeyError {
    #[error("invalid verifying key bytes: {0}")]
    InvalidVerifyingKey(ed25519_dalek::SignatureError),
}

pub mod time_helpers {
    use super::*;
    use time::macros::format_description;

    pub fn parse_offset_datetime(s: &str) -> Result<OffsetDateTime, time::Error> {
        OffsetDateTime::parse(
            s,
            &format_description!("[year]-[month]-[day]T[hour]:[minute]:[second]Z"),
        )
    }

    pub fn format_offset_datetime(t: OffsetDateTime) -> Result<String, time::Error> {
        let fmt = format_description!("[year]-[month]-[day]T[hour]:[minute]:[second]Z");
        t.format(&fmt)
    }

    pub fn naive_to_offset(naive: PrimitiveDateTime, offset_seconds: i32) -> OffsetDateTime {
        let offset =
            time::UtcOffset::from_seconds(offset_seconds).unwrap_or(time::UtcOffset::UTC);
        naive.assume_offset(offset)
    }
}
