use serde::{Deserialize, Serialize};
use time::OffsetDateTime;
use uuid::Uuid;

use aln_core::{Did, HexHash};
use prometheus_praxis_spine::{KER, Residual, CorridorBand, EcoCredit};

/// Corridor bands for a plane (helper type).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CorridorBands {
    pub plane: String,
    pub bands: Vec<CorridorBand>,
}

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
    /// Optional eco-credit computed from this action's Lyapunov improvement.
    /// Credits are output-only and MUST NOT be fed back into control logic.
    pub eco_credit: Option<EcoCredit>,
}

/// Compute eco-credit for a ledger particle based on Lyapunov delta.
///
/// This function is the canonical way for ledger crates to compute eco-restoration credits.
/// Credits are recorded for reporting and policy metrics only and are never fed back
/// into control logic or plane weights.
pub fn compute_eco_credit_for_ledger(
    delta_v: f64,
    r_carbon: f64,
    jw: f64,
    nonoffsettable_compliant: bool,
) -> Option<EcoCredit> {
    EcoCredit::mint(delta_v, r_carbon, jw, nonoffsettable_compliant)
}
