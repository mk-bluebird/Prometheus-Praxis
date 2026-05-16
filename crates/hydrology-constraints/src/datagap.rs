use serde::{Deserialize, Serialize};
use time::OffsetDateTime;
use uuid::Uuid;
use aln_core::{Did, HexHash};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DataGapParticle {
    pub gap_id: Uuid,
    pub region_id: String,
    pub aquifer: Option<String>,
    pub variable_name: String,      // e.g. "recharge_rate_m3_per_day"
    pub created_at: OffsetDateTime,
    pub requested_by: Did,
    pub bounty_description: String,
    pub evidence_hash: HexHash,     // hash of the gap spec / call
}
