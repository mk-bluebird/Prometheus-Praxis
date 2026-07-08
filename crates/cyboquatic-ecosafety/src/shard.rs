// filename: crates/cyboquatic-ecosafety/src/shard.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

use crate::error::FrameError;

/// Ecosafety shard update record.
///
/// This struct carries updated risk metrics, K/E/R snapshots, and a
/// structured error list for partial or failed evaluations.
#[derive(Debug, Clone)]
pub struct ShardUpdate {
    pub shard_id: String,
    pub node_id: String,
    pub region_id: String,
    pub ker_snapshot_before: Option<KerSnapshot>,
    pub ker_snapshot_after: Option<KerSnapshot>,
    pub errors: Vec<FrameError>,
    // Other fields: timestamps, evidence hex, provenance chain, etc.
}

/// Minimal KER snapshot type; the full version should mirror ALN
/// KerSnapshot2026v1 semantics.
#[derive(Debug, Clone)]
pub struct KerSnapshot {
    pub k: f32,
    pub e: f32,
    pub r: f32,
    pub vt: f32,
    pub lane: Lane,
    pub is_speculative: bool,
}

#[derive(Debug, Clone, Copy)]
pub enum Lane {
    Research,
    Exp,
    Sim,
    Prod,
}

impl ShardUpdate {
    pub fn new(shard_id: String, node_id: String, region_id: String) -> Self {
        Self {
            shard_id,
            node_id,
            region_id,
            ker_snapshot_before: None,
            ker_snapshot_after: None,
            errors: Vec::new(),
        }
    }

    pub fn add_error(&mut self, err: FrameError) {
        self.errors.push(err);
    }

    pub fn has_errors(&self) -> bool {
        !self.errors.is_empty()
    }
}
