use ed25519_dalek::{Signature, SigningKey, VerifyingKey};
use rand::rngs::OsRng;
use serde::{Deserialize, Serialize};

/// HexCommit is a tamper-evident commitment string.
/// This implementation uses Ed25519 signatures encoded as hex.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HexCommit {
    pub hex: String,
}

pub struct HexCommitSigner {
    signing_key: SigningKey,
}

pub struct HexCommitVerifier {
    verifying_key: VerifyingKey,
}

impl HexCommitSigner {
    pub fn new() -> Self {
        let signing_key = SigningKey::generate(&mut OsRng);
        Self { signing_key }
    }

    pub fn commit(&self, payload: &[u8]) -> HexCommit {
        let sig: Signature = self.signing_key.sign(payload);
        let hex = hex::encode(sig.to_bytes());
        HexCommit { hex }
    }

    pub fn verifying_key(&self) -> VerifyingKey {
        self.signing_key.verifying_key()
    }
}

impl HexCommitVerifier {
    pub fn new(verifying_key: VerifyingKey) -> Self {
        Self { verifying_key }
    }

    pub fn verify(&self, payload: &[u8], commit: &HexCommit) -> bool {
        if let Ok(bytes) = hex::decode(&commit.hex) {
            if let Ok(sig) = Signature::from_bytes(&bytes.try_into().unwrap_or([0u8; 64])) {
                return self.verifying_key.verify_strict(payload, &sig).is_ok();
            }
        }
        false
    }
}
