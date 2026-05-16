use serde::{Deserialize, Serialize};
use time::OffsetDateTime;
use uuid::Uuid;

use aln_core::HexHash;
use ecospine::KER;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum EnergyDomain {
    Actual,
    Modeled,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoperJouleRecord {
    pub record_id: Uuid,
    pub workload_id: String,      // e.g. "Cyboquatic_Node_42"
    pub node_id: Uuid,
    pub timestamp: OffsetDateTime,
    pub karmadelta: f64,          // K gain or ecoimpact delta proxy
    pub energy_joules: f64,       // measured or modeled energy consumption
    pub ecoper_joule: f64,        // karmadelta / energy_joules
    pub energy_domain: EnergyDomain,
    pub qpu_catalog_entry: Option<Uuid>,
    pub tags: Vec<String>,
    pub evidence_hash: HexHash,   // hex of joined K/energy evidence
    pub ker_snapshot: KER,        // KER at this workload placement
}
