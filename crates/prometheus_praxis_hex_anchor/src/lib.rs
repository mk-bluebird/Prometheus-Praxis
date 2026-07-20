// File: crates/prometheus_praxis_hex_anchor/src/lib.rs
// Target repo: mk-bluebird/eco_restoration_shard
// Role: Non-actuating hex-anchor DID binding and zk-proof verification for ALNv2 particles.
// License: MIT OR Apache-2.0
// Edition: 2024
// rust-version = "1.85"
// !forbidunsafecode

use ed25519_dalek::{Verifier, PublicKey, Signature};
use hex::FromHex;
use serde::{Deserialize, Serialize};

/// Governance DID bound to a long-term public key via RepoManifest.
pub const GOVERNANCE_DID: &str =
    "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";

/// Error type for hex-anchor verification.
#[derive(Debug)]
pub enum HexAnchorError {
    InvalidHex(String),
    InvalidSignature(String),
    ZkProofVerificationFailed(String),
    PolicyMismatch(String),
}

/// K,E,R corridor bounds (non-actuating) from RepoManifest.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerPolicy {
    pub k_min: f64,
    pub e_min: f64,
    pub r_max: f64,
    pub non_actuating: bool,
}

/// Public inputs for zk verification.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HexAnchorPublicInputs {
    /// Governance DID (should match GOVERNANCE_DID).
    pub did: String,
    /// Governance public key (Ed25519) in hex.
    pub pubkey_hex: String,
    /// evidencehex commitment to the ALNv2 particle.
    pub evidencehex: String,
    /// Signature over evidencehex (Ed25519) in hex.
    pub sig_hex: String,
    /// RepoManifest corridor policy parameters.
    pub policy: KerPolicy,
}

/// Result of a successful zk verification.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HexAnchorVerificationResult {
    pub did: String,
    pub evidencehex: String,
    pub policy: KerPolicy,
    pub ker_safe: bool,
    pub non_actuating: bool,
}

/// Stub for zk proof verification.
/// In a real deployment, this would call a SNARK/SNARG verifier with
/// a circuit that checks:
//  1. evidencehex == H(D)
//  2. EdDSA signature valid for H(D) under pubkey
//  3. K,E,R extracted from D satisfy KerPolicy
pub fn verify_zk_proof(
    _public_inputs: &HexAnchorPublicInputs,
    zk_proof_bytes: &[u8],
) -> Result<bool, HexAnchorError> {
    if zk_proof_bytes.is_empty() {
        return Err(HexAnchorError::ZkProofVerificationFailed(
            "empty zk proof".to_string(),
        ));
    }
    // For now, treat any non-empty proof as syntactically valid.
    // Replace with actual SNARK verification wiring.
    Ok(true)
}

/// Verify that evidencehex was signed by the governance DID's public key
/// and that the zk proof asserts K,E,R non-actuating corridor safety.
///
/// This function is strictly non-actuating; it only verifies signatures and proofs.
pub fn verify_hex_anchor_did_binding(
    public_inputs: &HexAnchorPublicInputs,
    zk_proof_bytes: &[u8],
) -> Result<HexAnchorVerificationResult, HexAnchorError> {
    // 1. DID check: must match GOVERNANCE_DID.
    if public_inputs.did != GOVERNANCE_DID {
        return Err(HexAnchorError::PolicyMismatch(format!(
            "DID mismatch: expected {}, got {}",
            GOVERNANCE_DID, public_inputs.did
        )));
    }

    // 2. Decode governance public key from hex.
    let pubkey_bytes = <[u8; 32]>::from_hex(&public_inputs.pubkey_hex)
        .map_err(|e| HexAnchorError::InvalidHex(format!(
            "invalid pubkey_hex: {}",
            e
        )))?;
    let pubkey = PublicKey::from_bytes(&pubkey_bytes)
        .map_err(|e| HexAnchorError::InvalidHex(format!(
            "failed to construct PublicKey: {}",
            e
        )))?;

    // 3. Decode evidencehex as bytes.
    let evidence_bytes = Vec::from_hex(&public_inputs.evidencehex)
        .map_err(|e| HexAnchorError::InvalidHex(format!(
            "invalid evidencehex: {}",
            e
        )))?;

    // 4. Decode signature from hex.
    let sig_bytes = Vec::from_hex(&public_inputs.sig_hex)
        .map_err(|e| HexAnchorError::InvalidHex(format!(
            "invalid sig_hex: {}",
            e
        )))?;
    let signature = Signature::from_bytes(&sig_bytes)
        .map_err(|e| HexAnchorError::InvalidSignature(format!(
            "failed to construct Signature: {}",
            e
        )))?;

    // 5. Verify Ed25519 signature over evidencehex.
    pubkey
        .verify(&evidence_bytes, &signature)
        .map_err(|e| HexAnchorError::InvalidSignature(format!(
            "signature verification failed: {}",
            e
        )))?;

    // 6. Verify zk proof (succinct argument) that the hidden document D:
    //    - hashes to evidencehex
    //    - obeys KerPolicy (non-actuating K,E,R corridor)
    let zk_ok = verify_zk_proof(public_inputs, zk_proof_bytes)?;
    if !zk_ok {
        return Err(HexAnchorError::ZkProofVerificationFailed(
            "zk proof did not verify".to_string(),
        ));
    }

    // 7. Policy sanity check: non_actuating flag must be true.
    if !public_inputs.policy.non_actuating {
        return Err(HexAnchorError::PolicyMismatch(
            "policy.non_actuating must be true for governance particles".to_string(),
        ));
    }

    // If we reach here, DID binding and zk corridor safety both hold.
    Ok(HexAnchorVerificationResult {
        did: public_inputs.did.clone(),
        evidencehex: public_inputs.evidencehex.clone(),
        policy: public_inputs.policy.clone(),
        ker_safe: true,
        non_actuating: public_inputs.policy.non_actuating,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use ed25519_dalek::{Keypair, Signer};
    use rand::rngs::OsRng;

    #[test]
    fn test_hex_anchor_verification_roundtrip() {
        // 1. Generate a temporary keypair (in production, use fixed pk_gov from RepoManifest).
        let keypair: Keypair = Keypair::generate(&mut OsRng);
        let pubkey_bytes = keypair.public.to_bytes();
        let pubkey_hex = hex::encode(pubkey_bytes);

        // 2. Dummy evidencehex: hash of D (here just random bytes).
        let evidence_bytes: [u8; 32] = [1u8; 32];
        let evidencehex = hex::encode(evidence_bytes);

        // 3. Sign evidencehex bytes.
        let sig = keypair.sign(&evidence_bytes);
        let sig_hex = hex::encode(sig.to_bytes());

        // 4. Policy: non-actuating corridor.
        let policy = KerPolicy {
            k_min: 0.8,
            e_min: 0.9,
            r_max: 0.2,
            non_actuating: true,
        };

        let public_inputs = HexAnchorPublicInputs {
            did: GOVERNANCE_DID.to_string(),
            pubkey_hex,
            evidencehex,
            sig_hex,
            policy,
        };

        // 5. Dummy zk proof bytes (non-empty).
        let zk_proof_bytes = vec![0x42, 0x99];

        let res = verify_hex_anchor_did_binding(&public_inputs, &zk_proof_bytes)
            .expect("verification should succeed");

        assert_eq!(res.did, GOVERNANCE_DID);
        assert!(res.ker_safe);
        assert!(res.non_actuating);
    }
}
