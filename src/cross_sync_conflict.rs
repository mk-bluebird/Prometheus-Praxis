// filename: src/cross_sync_conflict.rs
// destination: eco_restoration_shard/src/cross_sync_conflict.rs

use serde::{Deserialize, Serialize};
use uuid::Uuid;

use aln_core::HexHash;
use ecospine::KER;

/// 9. Conflict resolution for CrossSync (CRDT-style with KER-weighted trust)

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PortfolioVersion {
    pub shard_id: Uuid,
    pub region_id: String,
    pub steward_did: String,
    pub ker: KER,
    pub eco_wealth_hash: HexHash,
    pub logical_clock: i64, // lamport or wall-clock
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConflictResolutionDecision {
    pub canonical_shard_id: Uuid,
    pub reason: String,
}

pub fn resolve_conflict(
    a: &PortfolioVersion,
    b: &PortfolioVersion,
) -> ConflictResolutionDecision {
    // KER-weighted trust, fall back to last-writer-wins
    let trust_a = a.ker.k * a.ker.e * (1.0 - a.ker.r);
    let trust_b = b.ker.k * b.ker.e * (1.0 - b.ker.r);

    if (trust_a - trust_b).abs() > 0.01 {
        if trust_a > trust_b {
            ConflictResolutionDecision {
                canonical_shard_id: a.shard_id,
                reason: "higher KER-weighted trust".to_string(),
            }
        } else {
            ConflictResolutionDecision {
                canonical_shard_id: b.shard_id,
                reason: "higher KER-weighted trust".to_string(),
            }
        }
    } else if a.logical_clock >= b.logical_clock {
        ConflictResolutionDecision {
            canonical_shard_id: a.shard_id,
            reason: "last-writer-wins after equal trust".to_string(),
        }
    } else {
        ConflictResolutionDecision {
            canonical_shard_id: b.shard_id,
            reason: "last-writer-wins after equal trust".to_string(),
        }
    }
}
