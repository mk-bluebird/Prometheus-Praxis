use serde::{Deserialize, Serialize};
use time::OffsetDateTime;
use uuid::Uuid;

use aln_core::{Did, HexHash};
use ecospine::{KER, Residual, CorridorBands};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DeliverableLink {
    pub uri: String,
    pub mime_type: String,
    pub description: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RegionalEcoLedgerParticle {
    pub particle_id: Uuid,
    pub region_id: String,          // basin or administrative code
    pub action_type: String,        // "irrigation_repair", "invasive_removal", etc.
    pub actor_did: Did,             // decentralized identifier of steward
    pub timestamp: OffsetDateTime,
    pub ker: KER,                   // full K,E,R triad
    pub residual_before: Residual,
    pub residual_after: Residual,
    pub corridor_bands: Vec<CorridorBands>,
    pub deliverables: Vec<DeliverableLink>,
    pub evidence_hash: HexHash,     // hex hash of supporting data
    pub nonce: u64,
    pub created_at: OffsetDateTime,
}
