// File: crates/prometheus_praxis_hex_anchor/src/lib.rs
#![forbid(unsafe_code)]

pub mod canal_node_anchor;

use ed25519_dalek::{Signature, Verifier, VerifyingKey};
use serde::{Deserialize, Serialize};
use std::{error::Error, fmt};

pub use canal_node_anchor::{
    hex_distance, CanalAnchorError, CanalNodeAnchor, CanalNodeAnchorIndex, HexGridCoordinate,
};

pub const GOVERNANCE_DID: &str = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
const KER_PROOF_DOMAIN: &[u8] = b"prometheus-praxis-ker-attestation-v1";
const KER_PROOF_BYTES: usize = 24 + 64;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum HexAnchorError {
    InvalidHex(String),
    InvalidSignature(String),
    InvalidProof(String),
    PolicyMismatch(String),
}

impl fmt::Display for HexAnchorError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidHex(message)
            | Self::InvalidSignature(message)
            | Self::InvalidProof(message)
            | Self::PolicyMismatch(message) => formatter.write_str(message),
        }
    }
}

impl Error for HexAnchorError {}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct KerPolicy {
    pub k_min: f64,
    pub e_min: f64,
    pub r_max: f64,
    pub non_actuating: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct HexAnchorPublicInputs {
    pub did: String,
    pub pubkey_hex: String,
    pub evidencehex: String,
    pub sig_hex: String,
    pub policy: KerPolicy,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct HexAnchorVerificationResult {
    pub did: String,
    pub evidencehex: String,
    pub policy: KerPolicy,
    pub ker_k: f64,
    pub ker_e: f64,
    pub ker_r: f64,
    pub ker_safe: bool,
    pub non_actuating: bool,
}

#[derive(Clone, Copy)]
struct KerAttestation {
    k: f64,
    e: f64,
    r: f64,
    signature: Signature,
}

fn decode_hex(value: &str, field: &str) -> Result<Vec<u8>, HexAnchorError> {
    let normalized = value.strip_prefix("0x").unwrap_or(value);
    hex::decode(normalized)
        .map_err(|error| HexAnchorError::InvalidHex(format!("invalid {field}: {error}")))
}

fn bounded_unit(value: f64) -> bool {
    value.is_finite() && (0.0..=1.0).contains(&value)
}

fn validate_policy(policy: &KerPolicy) -> Result<(), HexAnchorError> {
    if !policy.non_actuating {
        return Err(HexAnchorError::PolicyMismatch(
            "governance policy must be non-actuating".into(),
        ));
    }
    if !bounded_unit(policy.k_min) || !bounded_unit(policy.e_min) || !bounded_unit(policy.r_max) {
        return Err(HexAnchorError::PolicyMismatch(
            "KER policy bounds must be finite values in [0,1]".into(),
        ));
    }
    Ok(())
}

fn policy_message(evidence: &[u8], k: f64, e: f64, r: f64) -> Vec<u8> {
    let mut message = Vec::with_capacity(KER_PROOF_DOMAIN.len() + evidence.len() + 24);
    message.extend_from_slice(KER_PROOF_DOMAIN);
    message.extend_from_slice(evidence);
    message.extend_from_slice(&k.to_le_bytes());
    message.extend_from_slice(&e.to_le_bytes());
    message.extend_from_slice(&r.to_le_bytes());
    message
}

fn decode_attestation(bytes: &[u8]) -> Result<KerAttestation, HexAnchorError> {
    if bytes.len() != KER_PROOF_BYTES {
        return Err(HexAnchorError::InvalidProof(format!(
            "KER attestation must contain {KER_PROOF_BYTES} bytes"
        )));
    }

    let k = f64::from_le_bytes(bytes[0..8].try_into().expect("fixed slice length"));
    let e = f64::from_le_bytes(bytes[8..16].try_into().expect("fixed slice length"));
    let r = f64::from_le_bytes(bytes[16..24].try_into().expect("fixed slice length"));
    let signature = Signature::from_slice(&bytes[24..])
        .map_err(|error| HexAnchorError::InvalidProof(format!("invalid KER signature: {error}")))?;

    if !bounded_unit(k) || !bounded_unit(e) || !bounded_unit(r) {
        return Err(HexAnchorError::InvalidProof(
            "attested KER values must be finite values in [0,1]".into(),
        ));
    }

    Ok(KerAttestation { k, e, r, signature })
}

pub fn verify_ker_attestation(
    public_inputs: &HexAnchorPublicInputs,
    evidence: &[u8],
    verifying_key: &VerifyingKey,
    attestation_bytes: &[u8],
) -> Result<(f64, f64, f64), HexAnchorError> {
    let attestation = decode_attestation(attestation_bytes)?;
    let message = policy_message(evidence, attestation.k, attestation.e, attestation.r);

    verifying_key
        .verify(&message, &attestation.signature)
        .map_err(|error| {
            HexAnchorError::InvalidProof(format!("KER attestation signature failed: {error}"))
        })?;

    if attestation.k < public_inputs.policy.k_min
        || attestation.e < public_inputs.policy.e_min
        || attestation.r > public_inputs.policy.r_max
        || attestation.k * attestation.e <= attestation.r
    {
        return Err(HexAnchorError::PolicyMismatch(
            "attested KER values violate the declared corridor policy".into(),
        ));
    }

    Ok((attestation.k, attestation.e, attestation.r))
}

pub fn verify_hex_anchor_did_binding(
    public_inputs: &HexAnchorPublicInputs,
    ker_attestation_bytes: &[u8],
) -> Result<HexAnchorVerificationResult, HexAnchorError> {
    if public_inputs.did != GOVERNANCE_DID {
        return Err(HexAnchorError::PolicyMismatch(format!(
            "DID mismatch: expected {GOVERNANCE_DID}, got {}",
            public_inputs.did
        )));
    }
    validate_policy(&public_inputs.policy)?;

    let key_bytes = decode_hex(&public_inputs.pubkey_hex, "pubkey_hex")?;
    let key_array: [u8; 32] = key_bytes.try_into().map_err(|_| {
        HexAnchorError::InvalidHex("pubkey_hex must encode exactly 32 bytes".into())
    })?;
    let verifying_key = VerifyingKey::from_bytes(&key_array)
        .map_err(|error| HexAnchorError::InvalidHex(format!("invalid public key: {error}")))?;

    let evidence = decode_hex(&public_inputs.evidencehex, "evidencehex")?;
    if evidence.is_empty() {
        return Err(HexAnchorError::InvalidHex(
            "evidencehex must not be empty".into(),
        ));
    }

    let signature_bytes = decode_hex(&public_inputs.sig_hex, "sig_hex")?;
    let signature = Signature::from_slice(&signature_bytes).map_err(|error| {
        HexAnchorError::InvalidSignature(format!("invalid evidence signature: {error}"))
    })?;
    verifying_key.verify(&evidence, &signature).map_err(|error| {
        HexAnchorError::InvalidSignature(format!("evidence signature verification failed: {error}"))
    })?;

    let (ker_k, ker_e, ker_r) =
        verify_ker_attestation(public_inputs, &evidence, &verifying_key, ker_attestation_bytes)?;

    Ok(HexAnchorVerificationResult {
        did: public_inputs.did.clone(),
        evidencehex: public_inputs.evidencehex.clone(),
        policy: public_inputs.policy.clone(),
        ker_k,
        ker_e,
        ker_r,
        ker_safe: true,
        non_actuating: true,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use ed25519_dalek::{Signer, SigningKey};

    fn ker_attestation(
        signing_key: &SigningKey,
        evidence: &[u8],
        k: f64,
        e: f64,
        r: f64,
    ) -> Vec<u8> {
        let mut output = Vec::with_capacity(KER_PROOF_BYTES);
        output.extend_from_slice(&k.to_le_bytes());
        output.extend_from_slice(&e.to_le_bytes());
        output.extend_from_slice(&r.to_le_bytes());
        output.extend_from_slice(&signing_key.sign(&policy_message(evidence, k, e, r)).to_bytes());
        output
    }

    #[test]
    fn verifies_signed_evidence_and_attested_ker_corridor() {
        let signing_key = SigningKey::from_bytes(&[7_u8; 32]);
        let evidence = [1_u8; 32];
        let policy = KerPolicy {
            k_min: 0.80,
            e_min: 0.85,
            r_max: 0.20,
            non_actuating: true,
        };
        let public_inputs = HexAnchorPublicInputs {
            did: GOVERNANCE_DID.into(),
            pubkey_hex: hex::encode(signing_key.verifying_key().to_bytes()),
            evidencehex: format!("0x{}", hex::encode(evidence)),
            sig_hex: hex::encode(signing_key.sign(&evidence).to_bytes()),
            policy,
        };
        let result = verify_hex_anchor_did_binding(
            &public_inputs,
            &ker_attestation(&signing_key, &evidence, 0.93, 0.90, 0.12),
        )
        .unwrap();

        assert!(result.ker_safe);
        assert_eq!(result.ker_k, 0.93);
        assert_eq!(result.ker_e, 0.90);
        assert_eq!(result.ker_r, 0.12);
        assert!(result.non_actuating);
    }

    #[test]
    fn rejects_attestation_that_fails_ker_margin() {
        let signing_key = SigningKey::from_bytes(&[9_u8; 32]);
        let evidence = [2_u8; 32];
        let public_inputs = HexAnchorPublicInputs {
            did: GOVERNANCE_DID.into(),
            pubkey_hex: hex::encode(signing_key.verifying_key().to_bytes()),
            evidencehex: hex::encode(evidence),
            sig_hex: hex::encode(signing_key.sign(&evidence).to_bytes()),
            policy: KerPolicy {
                k_min: 0.50,
                e_min: 0.50,
                r_max: 0.40,
                non_actuating: true,
            },
        };

        let result = verify_hex_anchor_did_binding(
            &public_inputs,
            &ker_attestation(&signing_key, &evidence, 0.50, 0.50, 0.25),
        );
        assert!(matches!(result, Err(HexAnchorError::PolicyMismatch(_))));
    }
}
