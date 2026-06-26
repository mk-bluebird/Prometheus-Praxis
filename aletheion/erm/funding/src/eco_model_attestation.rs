// Path: aletheion/erm/funding/src/eco_model_attestation.rs

use rust_decimal::Decimal;
use chrono::{DateTime, Utc};
use serde::{Serialize, Deserialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct EcoModelAttestation {
    pub attestation_id: String,         // hex/compact id
    pub model_kind: String,            // "QF_MATCH_HEALTH_V1"
    pub campaign_id: String,
    pub ker_k: Decimal,
    pub ker_e: Decimal,
    pub ker_r: Decimal,
    pub qf_pool_native: Decimal,       // total matching pool
    pub qf_match_per_campaign: Vec<QfMatchPerCampaign>,
    pub computed_at_utc: DateTime<Utc>,
    pub operator_did: String,          // e.g. bostrom18...
    pub msg_publish_id: String,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct QfMatchPerCampaign {
    pub campaign_id: String,
    pub match_amount_native: Decimal,
    pub qf_sum_sqrt: Decimal,
    pub qf_match_weight: Decimal,
}
