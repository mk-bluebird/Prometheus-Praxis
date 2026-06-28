// filename: crates/vocab_guard/src/lib.rs
#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};
use thiserror::Error;

/// Lane marks how aligned a shard is with hardened grammar.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum Lane {
    Research,
    Pilot,
    Prod,
    Exploratory,
}

/// Simple K/E/R triad for vocabulary shards.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct KerScore {
    pub knowledge_factor: f32,   // 0.0 .. 1.0
    pub eco_impact_value: f32,   // 0.0 .. 1.0
    pub risk_of_harm: f32,       // 0.0 .. 1.0
}

/// Minimal identity binding to your Bostrom / EVM addresses.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IdentityBinding {
    pub logical_name: String,
    pub did_primary: String,
    pub did_alt: String,
    pub evm_wallet: String,
}

/// Canonical vocabulary shard: one concept in one grammar.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VocabShard {
    /// Stable machine name for the concept, e.g. "EcoLiabilityShard".
    pub term_id: String,
    /// Human-facing description; must be canonical, not per-project slang.
    pub description: String,
    /// Core grammar channel, e.g. "materials", "hydrology", "governance".
    pub plane: String,
    /// Lane alignment.
    pub lane: Lane,
    /// K/E/R scoring for data-as-labor.
    pub ker: KerScore,
    /// Sovereign identity binding.
    pub identity: IdentityBinding,
    /// Evidence hexstamp anchoring this shard to qpudatashards/evidence bundles.
    pub evidence_hex: String,
}

#[derive(Debug, Error)]
pub enum VocabError {
    #[error("term_id must not be empty")]
    EmptyTermId,
    #[error("description must not be empty")]
    EmptyDescription,
    #[error("plane must not be empty")]
    EmptyPlane,
    #[error("knowledge_factor out of range [0,1]: {0}")]
    BadKnowledge(f32),
    #[error("eco_impact_value out of range [0,1]: {0}")]
    BadEcoImpact(f32),
    #[error("risk_of_harm out of range [0,1]: {0}")]
    BadRisk(f32),
    #[error("primary DID must not be empty")]
    EmptyDidPrimary,
    #[error("evidence_hex must not be empty")]
    EmptyEvidenceHex,
}

/// Deterministic guard: checks that a shard fits the one-vocabulary policy.
pub fn validate_vocab_shard(shard: &VocabShard) -> Result<(), VocabError> {
    if shard.term_id.trim().is_empty() {
        return Err(VocabError::EmptyTermId);
    }
    if shard.description.trim().is_empty() {
        return Err(VocabError::EmptyDescription);
    }
    if shard.plane.trim().is_empty() {
        return Err(VocabError::EmptyPlane);
    }
    if shard.ker.knowledge_factor < 0.0 || shard.ker.knowledge_factor > 1.0 {
        return Err(VocabError::BadKnowledge(shard.ker.knowledge_factor));
    }
    if shard.ker.eco_impact_value < 0.0 || shard.ker.eco_impact_value > 1.0 {
        return Err(VocabError::BadEcoImpact(shard.ker.eco_impact_value));
    }
    if shard.ker.risk_of_harm < 0.0 || shard.ker.risk_of_harm > 1.0 {
        return Err(VocabError::BadRisk(shard.ker.risk_of_harm));
    }
    if shard.identity.did_primary.trim().is_empty() {
        return Err(VocabError::EmptyDidPrimary);
    }
    if shard.evidence_hex.trim().is_empty() {
        return Err(VocabError::EmptyEvidenceHex);
    }
    Ok(())
}

/// Example constructor wiring your primary identity into the shard.
/// You can call this whenever you define a new off-road term.
pub fn new_offroad_term(
    term_id: &str,
    description: &str,
    plane: &str,
    lane: Lane,
    ker: KerScore,
    evidence_hex: &str,
) -> VocabShard {
    let identity = IdentityBinding {
        logical_name: "ecorestorationshardprimary".to_string(),
        did_primary: "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7".to_string(),
        did_alt: "bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc".to_string(),
        evm_wallet: "0x519fC0eB4111323Cac44b70e1aE31c30e405802D".to_string(),
    };

    VocabShard {
        term_id: term_id.to_string(),
        description: description.to_string(),
        plane: plane.to_string(),
        lane,
        ker,
        identity,
        evidence_hex: evidence_hex.to_string(),
    }
}

/// JSON helper so AI-chat and tools can crawl the vocabulary tree.
pub fn shard_to_json(shard: &VocabShard) -> Result<String, serde_json::Error> {
    serde_json::to_string_pretty(shard)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_valid_shard() {
        let ker = KerScore {
            knowledge_factor: 0.92,
            eco_impact_value: 0.90,
            risk_of_harm: 0.12,
        };
        let shard = new_offroad_term(
            "BiodegradableSubstrateCorridor",
            "Corridor describing decomposition-safe substrate for recycled materials.",
            "materials",
            Lane::Exploratory,
            ker,
            "deadbeef01",
        );
        assert!(validate_vocab_shard(&shard).is_ok());
        let json = shard_to_json(&shard).unwrap();
        assert!(json.contains("BiodegradableSubstrateCorridor"));
    }

    #[test]
    fn test_bad_ker() {
        let ker = KerScore {
            knowledge_factor: 1.2,
            eco_impact_value: 0.90,
            risk_of_harm: 0.12,
        };
        let shard = new_offroad_term(
            "BadShard",
            "Invalid KER shard.",
            "materials",
            Lane::Research,
            ker,
            "cafebabe01",
        );
        let err = validate_vocab_shard(&shard).unwrap_err();
        match err {
            VocabError::BadKnowledge(_) => {}
            _ => panic!("expected BadKnowledge error"),
        }
    }
}
