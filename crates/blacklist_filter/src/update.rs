// filename: crates/blacklist_filter/src/update.rs
// destination: ecorestorationshard/crates/blacklist_filter/src/update.rs

use crate::{BlacklistConfig, BlacklistSnapshot, BloomFilter};
use std::time::{Duration, SystemTime};

/// JSON payload schema returned by the DAO:
/// {
///   "merkle_root_hex": "abcd...",
///   "bloom_bits_base64": "...",
///   "k": 4,
///   "valid_for_seconds": 86400
/// }
#[derive(serde::Deserialize)]
struct BlacklistResponse {
    merkle_root_hex: String,
    bloom_bits_base64: String,
    k: u32,
    valid_for_seconds: u64,
}

/// Fetch and decode a blacklist snapshot from the DAO.
pub fn fetch_blacklist_snapshot(
    cfg: &BlacklistConfig,
) -> Result<BlacklistSnapshot, Box<dyn std::error::Error>> {
    let url = cfg.endpoint_url();
    let resp = ureq::get(&url).call()?;
    if resp.status() != 200 {
        return Err(format!("blacklist fetch failed with status {}", resp.status()).into());
    }

    let body = resp.into_string()?;
    let decoded: BlacklistResponse = serde_json::from_str(&body)?;

    let bits = base64::decode(&decoded.bloom_bits_base64)?;
    let bloom_filter = BloomFilter::new(bits, decoded.k);

    Ok(BlacklistSnapshot {
        merkle_root_hex: decoded.merkle_root_hex,
        bloom_filter,
        fetched_at: SystemTime::now(),
        valid_for: Duration::from_secs(decoded.valid_for_seconds),
    })
}
