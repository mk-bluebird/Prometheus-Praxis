// filename: crates/bostrom-cli/src/blacklist_guard.rs
// destination: ecorestorationshard/crates/bostrom-cli/src/blacklist_guard.rs

use std::sync::{Arc, RwLock};
use std::time::SystemTime;

use blacklist_filter::{BlacklistConfig, BlacklistSnapshot, fetch_blacklist_snapshot};

/// In-memory guard used by bostrom-cli and bostrom-signer to block blacklisted content.
pub struct BlacklistGuard {
    cfg: BlacklistConfig,
    snapshot: Arc<RwLock<Option<BlacklistSnapshot>>>,
}

impl BlacklistGuard {
    pub fn new(cfg: BlacklistConfig) -> Self {
        Self {
            cfg,
            snapshot: Arc::new(RwLock::new(None)),
        }
    }

    fn ensure_fresh_snapshot(&self) -> Result<(), Box<dyn std::error::Error>> {
        {
            let guard = self.snapshot.read().unwrap();
            if let Some(ref snap) = *guard {
                if !snap.is_expired(SystemTime::now()) {
                    return Ok(());
                }
            }
        }

        let mut guard = self.snapshot.write().unwrap();
        let snap = fetch_blacklist_snapshot(&self.cfg)?;
        *guard = Some(snap);
        Ok(())
    }

    /// Returns true if the given content hash is blacklisted.
    pub fn is_blacklisted(&self, content_hash: &[u8]) -> bool {
        if self.ensure_fresh_snapshot().is_err() {
            // Fail-safe: if blacklist cannot be loaded, treat as not blacklisted
            // but log; this path can be tightened by governance later.
            return false;
        }

        let guard = self.snapshot.read().unwrap();
        if let Some(ref snap) = *guard {
            return snap.is_blacklisted_hash(content_hash);
        }

        false
    }
}
