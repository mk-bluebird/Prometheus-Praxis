// filename: src/signer/sign_tx.rs
// destination: eco_restoration_shard/src/signer/sign_tx.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

use crate::bci::BciInterface;
use crate::safety::errors::SabotageError;

pub struct BostromSigner<B: BciInterface> {
    pub bci: B,
    pub tx_backend: TxBackend,
}

pub struct TxBackend;

impl TxBackend {
    pub fn sign_tx_raw(&self, tx_bytes: &[u8]) -> Result<Vec<u8>, SabotageError> {
        Ok(tx_bytes.to_vec())
    }
}

impl<B: BciInterface> BostromSigner<B> {
    pub fn sign_tx_raw(&self, tx_bytes: &[u8]) -> Result<Vec<u8>, SabotageError> {
        let status = self.bci.verify_connection();
        if !status.ok {
            return Err(SabotageError::BciUnavailable {
                reason: status.message,
            });
        }

        self.tx_backend.sign_tx_raw(tx_bytes)
    }
}

// filename: src/bci/health.rs
// destination: eco_restoration_shard/src/bci/health.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

pub struct BciHealthStatus {
    pub ok: bool,
    pub message: String,
}

pub trait BciInterface {
    fn verify_connection(&self) -> BciHealthStatus;
}
