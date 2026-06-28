// filename: crates/vocab_guard/src/lib.rs
#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};
use thiserror::Error;

/// How strict to be when validating shards.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum FlexMode {
    Strict,
    Relaxed,
}

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

/// Minimal identity binding to your sovereign addresses.
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
    pub term_id: String,
    pub description: String,
    pub plane: String,
    pub lane: Lane,
    pub ker: KerScore,
    pub identity: IdentityBinding,
    /// May be empty in Relaxed mode; required in Strict.
    pub evidence_hex: String,
    /// Optional tags for tools and AI-chat convenience.
    pub tags: Vec<String>,
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
    #[error("evidence_hex must not be empty in Strict mode")]
    EmptyEvidenceHex,
}

/// Deterministic guard: checks that a shard fits the one-vocabulary policy.
/// FlexMode::Relaxed allows empty evidence_hex and looser K/E/R, but still
/// enforces basic bounds so AI-chat can use K/E/R safely.
pub fn validate_vocab_shard(shard: &VocabShard, mode: FlexMode) -> Result<(), VocabError> {
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
    if mode == FlexMode::Strict && shard.evidence_hex.trim().is_empty() {
        return Err(VocabError::EmptyEvidenceHex);
    }
    Ok(())
}

/// Convenience constructor for "draft" off-road terms with safe defaults.
/// These start as Exploratory lane and Relaxed K/E/R until you refine them.
pub fn new_draft_term(
    term_id: &str,
    description: &str,
    plane: &str,
    tags: &[&str],
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
        lane: Lane::Exploratory,
        ker: KerScore {
            knowledge_factor: 0.85,
            eco_impact_value: 0.85,
            risk_of_harm: 0.20,
        },
        identity,
        evidence_hex: String::new(), // empty in draft; fill when anchored
        tags: tags.iter().map(|t| t.to_string()).collect(),
    }
}

/// Constructor for hardened terms (Strict mode).
pub fn new_strict_term(
    term_id: &str,
    description: &str,
    plane: &str,
    lane: Lane,
    ker: KerScore,
    evidence_hex: &str,
    tags: &[&str],
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
        tags: tags.iter().map(|t| t.to_string()).collect(),
    }
}

/// JSON helpers so AI-chat and tools can crawl vocab easily.
pub fn shard_to_json(shard: &VocabShard) -> Result<String, serde_json::Error> {
    serde_json::to_string_pretty(shard)
}

/// Simple index type: a list of shards.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VocabIndex {
    pub shards: Vec<VocabShard>,
}

impl VocabIndex {
    pub fn new() -> Self {
        Self { shards: Vec::new() }
    }

    pub fn add(&mut self, shard: VocabShard) {
        self.shards.push(shard);
    }

    /// Filter shards by plane or tag (for user preference and ease-of-use).
    pub fn filter(&self, plane: Option<&str>, tag: Option<&str>) -> Vec<&VocabShard> {
        self.shards
            .iter()
            .filter(|s| {
                let plane_ok = plane.map(|p| s.plane == p).unwrap_or(true);
                let tag_ok = tag
                    .map(|t| s.tags.iter().any(|tg| tg == t))
                    .unwrap_or(true);
                plane_ok && tag_ok
            })
            .collect()
    }

    pub fn to_json(&self) -> Result<String, serde_json::Error> {
        serde_json::to_string_pretty(self)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_draft_term_relaxed_validation() {
        let shard = new_draft_term(
            "BiodegradableSubstrateCorridor",
            "Corridor describing decomposition-safe substrate for recycled materials.",
            "materials",
            &["biodegradable", "substrate", "tray"],
        );
        assert!(validate_vocab_shard(&shard, FlexMode::Relaxed).is_ok());
        let json = shard_to_json(&shard).unwrap();
        assert!(json.contains("BiodegradableSubstrateCorridor"));
    }

    #[test]
    fn test_strict_requires_evidence_hex() {
        let ker = KerScore {
            knowledge_factor: 0.92,
            eco_impact_value: 0.90,
            risk_of_harm: 0.12,
        };
        let shard = new_strict_term(
            "EcoDegradationKernel",
            "Kernel for biodegradable tray degradation corridors.",
            "materials",
            Lane::Pilot,
            ker,
            "",
            &["biodegradable", "kernel"],
        );
        let err = validate_vocab_shard(&shard, FlexMode::Strict).unwrap_err();
        matches!(err, VocabError::EmptyEvidenceHex);
    }

    #[test]
    fn test_index_filter_by_plane_and_tag() {
        let mut idx = VocabIndex::new();
        idx.add(new_draft_term("BioTray", "Biodegradable tray.", "materials", &["tray"]));
        idx.add(new_draft_term("LakeRisk", "Static lake risk vocab.", "hydrology", &["lake"]));

        let materials = idx.filter(Some("materials"), None);
        assert_eq!(materials.len(), 1);
        let tray_terms = idx.filter(None, Some("tray"));
        assert_eq!(tray_terms.len(), 1);
    }
}
