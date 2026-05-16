// filename: crates/blacklist_filter/src/lib.rs
// destination: ecorestorationshard/crates/blacklist_filter/src/lib.rs

use std::time::{Duration, SystemTime};

/// Compact Bloom filter for 256-bit content hashes.
pub struct BloomFilter {
    bits: Vec<u8>,
    k: u32,
}

impl BloomFilter {
    pub fn new(bits: Vec<u8>, k: u32) -> Self {
        Self { bits, k }
    }

    fn bit_len(&self) -> usize {
        self.bits.len() * 8
    }

    fn test_bit(&self, idx: usize) -> bool {
        let byte = idx / 8;
        let bit = idx % 8;
        if byte >= self.bits.len() {
            return false;
        }
        (self.bits[byte] & (1u8 << bit)) != 0
    }

    fn hash(&self, data: &[u8], i: u32) -> usize {
        use blake3::Hasher;

        let mut hasher = Hasher::new();
        hasher.update(data);
        hasher.update(&i.to_be_bytes());
        let digest = hasher.finalize();
        let mut bytes = [0u8; 8];
        bytes.copy_from_slice(&digest.as_bytes()[0..8]);
        let val = u64::from_be_bytes(bytes);
        (val as usize) % self.bit_len()
    }

    pub fn contains(&self, data: &[u8]) -> bool {
        for i in 0..self.k {
            let idx = self.hash(data, i);
            if !self.test_bit(idx) {
                return false;
            }
        }
        true
    }
}

/// Snapshot of blacklist metadata anchored by a Merkle root.
pub struct BlacklistSnapshot {
    pub merkle_root_hex: String,
    pub bloom_filter: BloomFilter,
    pub fetched_at: SystemTime,
    pub valid_for: Duration,
}

impl BlacklistSnapshot {
    pub fn is_expired(&self, now: SystemTime) -> bool {
        match self.fetched_at.checked_add(self.valid_for) {
            Some(expiry) => now > expiry,
            None => true,
        }
    }

    pub fn is_blacklisted_hash(&self, content_hash: &[u8]) -> bool {
        self.bloom_filter.contains(content_hash)
    }
}

/// API endpoint configuration for blacklist updates.
pub struct BlacklistConfig {
    pub dao_base_url: String,
    pub chain_id: String,
    pub list_name: String,
    pub update_interval: Duration,
}

impl BlacklistConfig {
    /// Construct the canonical endpoint URL:
    /// {dao_base_url}/blacklist/{chain_id}/{list_name}/latest.json
    pub fn endpoint_url(&self) -> String {
        format!(
            "{}/blacklist/{}/{}/latest.json",
            self.dao_base_url, self.chain_id, self.list_name
        )
    }
}
