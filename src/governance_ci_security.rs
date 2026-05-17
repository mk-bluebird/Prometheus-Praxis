// filename: src/governance_ci_security.rs
// destination: eco_restoration_shard/src/governance_ci_security.rs

use serde::{Deserialize, Serialize};
use uuid::Uuid;

use aln_core::{Did, HexHash};

/// 7. CI result signing by governance-held HSM

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CiKerResult {
    pub ci_run_id: Uuid,
    pub lane_id: Uuid,
    pub shard_window_hash: HexHash,
    pub k: f64,
    pub e: f64,
    pub r: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CiSignature {
    pub signer_did: Did,
    pub signature_hex: HexHash,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SignedCiResult {
    pub result: CiKerResult,
    pub signature: CiSignature,
}

/// CI outputs are only accepted if SignedCiResult is verifiable
/// against a governance-held HSM public key (out-of-band stored).
pub trait CiVerifier {
    fn verify(&self, signed: &SignedCiResult) -> bool;
}
