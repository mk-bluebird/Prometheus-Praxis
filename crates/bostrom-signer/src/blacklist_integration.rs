// filename: crates/bostrom-signer/src/blacklist_integration.rs
// destination: ecorestorationshard/crates/bostrom-signer/src/blacklist_integration.rs

use crate::{SignError, Signer};
use blacklist_filter::BlacklistConfig;
use sha2::{Digest, Sha256};

use std::sync::Arc;

use crate::blacklist_guard::BlacklistGuard;
use cosmos_sdk_proto::Any;

/// Signer wrapper that enforces blacklist checks before signing.
pub struct BlacklistAwareSigner<S: Signer> {
    inner: S,
    guard: Arc<BlacklistGuard>,
}

impl<S: Signer> BlacklistAwareSigner<S> {
    pub fn new(inner: S, cfg: BlacklistConfig) -> Self {
        let guard = Arc::new(BlacklistGuard::new(cfg));
        Self { inner, guard }
    }

    fn content_hash(&self, msg_any: &Any) -> Vec<u8> {
        let mut hasher = Sha256::new();
        hasher.update(msg_any.value.as_slice());
        hasher.finalize().to_vec()
    }
}

impl<S: Signer> Signer for BlacklistAwareSigner<S> {
    fn public_key(&self) -> &tendermint::crypto::Ed25519PublicKey {
        self.inner.public_key()
    }

    fn sign(&self, msg_any: &mut Any) -> Result<Vec<u8>, SignError> {
        let hash = self.content_hash(msg_any);
        if self.guard.is_blacklisted(&hash) {
            return Err(SignError::UnsupportedMessageType(
                "message content matches blacklisted pattern".to_string(),
            ));
        }

        self.inner.sign(msg_any)
    }
}
