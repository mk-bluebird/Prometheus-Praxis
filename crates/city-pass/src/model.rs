use serde::{Deserialize, Serialize};
use time::{OffsetDateTime, format_description::well_known::Rfc3339};
use uuid::Uuid;

/// Domain enumeration for capabilities.
/// Currently only OfflineTransit is allowed to keep the crate focused.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum CityCapabilityDomain {
    OfflineTransit,
}

/// Core sovereign capability — non-financial, non-somatic.
/// Design (D): Encodes only state needed for offline tap authorization.
/// NR: 0 — no neural or biometric fields.
/// EE: Encapsulates max_taps for local budgeting of energy-saving taps.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CityCapability {
    pub cap_id: Uuid,
    pub domain: CityCapabilityDomain,
    /// DID of the issuer (e.g. DID:ALN or DID:Web for city authority).
    pub issuer_did: String,
    /// Sovereign owner DID (host envelope DID).
    pub owner_did: String,
    /// Bostrom or compatible address for stake anchoring.
    pub owner_bostrom_addr: String,
    /// Eco contract binding (ALN shard id).
    pub eco_contract_id: String,
    /// Neurorights contract binding (ALN shard id).
    pub neurorights_contract_id: String,
    /// Jurisdictional policy identifier, e.g. policy.jurisdiction.us-az-maricopa-phoenix.v1
    pub jurisdiction_policy: String,
    /// UTC validity start (RFC3339).
    pub validity_start_utc: String,
    /// UTC validity end (RFC3339).
    pub validity_end_utc: String,
    /// Maximum allowed taps for this capability.
    pub max_taps: u32,
    /// Pre-computed hex_commit for tamper-evidence of this struct.
    pub hex_commit: String,
}

impl CityCapability {
    pub fn new_offline_transit(
        issuer_did: String,
        owner_did: String,
        owner_bostrom_addr: String,
        eco_contract_id: String,
        neurorights_contract_id: String,
        jurisdiction_policy: String,
        validity_start: OffsetDateTime,
        validity_end: OffsetDateTime,
        max_taps: u32,
        hex_commit: String,
    ) -> Self {
        Self {
            cap_id: Uuid::new_v4(),
            domain: CityCapabilityDomain::OfflineTransit,
            issuer_did,
            owner_did,
            owner_bostrom_addr,
            eco_contract_id,
            neurorights_contract_id,
            jurisdiction_policy,
            validity_start_utc: validity_start.format(&Rfc3339).unwrap(),
            validity_end_utc: validity_end.format(&Rfc3339).unwrap(),
            max_taps,
            hex_commit,
        }
    }

    pub fn validity_window(&self) -> Result<(OffsetDateTime, OffsetDateTime), time::Error> {
        let start = OffsetDateTime::parse(&self.validity_start_utc, &Rfc3339)?;
        let end = OffsetDateTime::parse(&self.validity_end_utc, &Rfc3339)?;
        Ok((start, end))
    }
}
