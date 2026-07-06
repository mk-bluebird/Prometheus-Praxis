// eco_restoration_shard/crates/prometheus_praxis_provenance/src/lib.rs
//
// Prometheus-Praxis Provenance Anchor
// Non-actuating, invariant-first provenance spine for governance decisions.
// Edition 2024, rust-version = "1.85", !forbid(unsafe_code) is allowed at crate level,
// but we explicitly forbid unsafe in this module to keep the logic verifiable.

#![forbid(unsafe_code)]

use chrono::{DateTime, Utc};
use rust_decimal::Decimal;
use serde::{Deserialize, Serialize};

/// GovernanceDecision is re-declared here to keep this crate self-contained.
/// In the mono-repo, you may instead `pub use prometheus_praxis::GovernanceDecision;`.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum GovernanceDecision {
    Allow,
    Derate,
    Stop,
}

/// High-level domain for a governed action, mirrored from the core kernel.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum ActionDomain {
    EcoRestoration,
    CityOperations,
    CosmicEnergy,
    MacroHealth,
}

/// Lane (RESEARCH, PILOT, PRODUCTION) for maturity/risk semantics.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum ActionLane {
    Research,
    Pilot,
    Production,
}

/// KER triad snapshot.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerSnapshot {
    pub k: Decimal,
    pub e: Decimal,
    pub r: Decimal,
}

/// Lyapunov residual snapshot.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LyapunovResidualSnapshot {
    pub vcurrent: Decimal,
    pub vnext: Decimal,
    pub epsilon: Decimal,
}

/// Risk-of-harm snapshot.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RohSnapshot {
    pub roh: Decimal,
    pub domain: ActionDomain,
    pub lane: ActionLane,
}

/// ALN shard identifier (envelope/corridor/contract reference).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AlnShardId {
    pub name: String,
    pub version: String,
}

/// EU AI Act risk level metadata (secondary constraint).
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum EuAiRiskLevel {
    Unacceptable,
    High,
    Limited,
    Minimal,
}

/// Lightweight source metadata for provenance (agent/tool/user/evidence).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SourceMeta {
    pub source_id: String,
    pub source_kind: String, // e.g. "mcp_tool", "human_operator", "batch_job"
    pub schema_version: String,
    pub content_hash_hex: String,
}

/// Provenance record anchored for every governance decision.
/// This is append-only and non-actuating by design.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProvenanceRecord {
    pub record_id: String,
    pub decision_id: String,
    pub macro_action_id: String,
    pub domain: ActionDomain,
    pub lane: ActionLane,
    pub ker: KerSnapshot,
    pub roh: RohSnapshot,
    pub lyapunov: LyapunovResidualSnapshot,
    pub tsafe_corridor_id: Option<String>,
    pub alnenvelope: AlnShardId,
    pub governance_decision: GovernanceDecision,
    pub eu_ai_risk: EuAiRiskLevel,
    pub created_at: DateTime<Utc>,
    pub source_meta: SourceMeta,
}

/// Anchor identifiers for Veritas-Chain and Janus-Veritas.
/// These are logical IDs only; the actual ledger clients live elsewhere.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VeritasAnchorId {
    pub chain_id: String,
    pub anchor_hex: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct JanusAnchorId {
    pub chain_id: String,
    pub anchor_hex: String,
}

/// Error type for provenance anchoring and retrieval logic.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ProvenanceError {
    InvalidRecord(String),
    AnchorFailure(String),
    NotFound(String),
}

/// Pure builder for ProvenanceRecord, enforcing minimal structural invariants.
pub fn build_provenance_record(
    decision_id: &str,
    macro_action_id: &str,
    domain: ActionDomain,
    lane: ActionLane,
    ker: KerSnapshot,
    roh: RohSnapshot,
    lyapunov: LyapunovResidualSnapshot,
    tsafe_corridor_id: Option<String>,
    alnenvelope: AlnShardId,
    governance_decision: GovernanceDecision,
    eu_ai_risk: EuAiRiskLevel,
    source_meta: SourceMeta,
) -> Result<ProvenanceRecord, ProvenanceError> {
    if decision_id.is_empty() {
        return Err(ProvenanceError::InvalidRecord(
            "decision_id must not be empty".to_string(),
        ));
    }
    if macro_action_id.is_empty() {
        return Err(ProvenanceError::InvalidRecord(
            "macro_action_id must not be empty".to_string(),
        ));
    }
    if alnenvelope.name.is_empty() || alnenvelope.version.is_empty() {
        return Err(ProvenanceError::InvalidRecord(
            "alnenvelope.name/version must not be empty".to_string(),
        ));
    }

    let record_id = format!("PRV-{}-{}", macro_action_id, decision_id);

    Ok(ProvenanceRecord {
        record_id,
        decision_id: decision_id.to_string(),
        macro_action_id: macro_action_id.to_string(),
        domain,
        lane,
        ker,
        roh,
        lyapunov,
        tsafe_corridor_id,
        alnenvelope,
        governance_decision,
        eu_ai_risk,
        created_at: Utc::now(),
        source_meta,
    })
}

/// Append a provenance record to Veritas-Chain.
/// Non-actuating: this function prepares payload; a separate client sends it.
pub fn append_veritas_chain(
    record: &ProvenanceRecord,
    target_chain_id: &str,
) -> Result<VeritasAnchorId, ProvenanceError> {
    if target_chain_id.is_empty() {
        return Err(ProvenanceError::AnchorFailure(
            "target_chain_id must not be empty".to_string(),
        ));
    }

    // In a full implementation, this would serialize `record` to ALN/JSON
    // and hand it off to a Veritas-Chain client. Here we derive a deterministic
    // logical anchor ID based on record fields.
    let anchor_hex = format!(
        "veritas:{}:{}:{}",
        record.macro_action_id, record.decision_id, record.domain as u8
    );

    Ok(VeritasAnchorId {
        chain_id: target_chain_id.to_string(),
        anchor_hex,
    })
}

/// Append a provenance record to Janus-Veritas for dual integrity/factuality anchoring.
pub fn append_janus_veritas(
    record: &ProvenanceRecord,
    target_chain_id: &str,
) -> Result<JanusAnchorId, ProvenanceError> {
    if target_chain_id.is_empty() {
        return Err(ProvenanceError::AnchorFailure(
            "target_chain_id must not be empty".to_string(),
        ));
    }

    let anchor_hex = format!(
        "janus:{}:{}:{}",
        record.macro_action_id, record.decision_id, record.eu_ai_risk as u8
    );

    Ok(JanusAnchorId {
        chain_id: target_chain_id.to_string(),
        anchor_hex,
    })
}

/// Query helper signatures (non-actuating): in a full system, these would
/// hit an indexed store; here we just define the API.
pub fn query_provenance_by_action(
    _macro_action_id: &str,
) -> Result<Vec<ProvenanceRecord>, ProvenanceError> {
    Err(ProvenanceError::NotFound(
        "query_provenance_by_action is not implemented in this stub".to_string(),
    ))
}

pub fn query_provenance_by_window(
    _start: DateTime<Utc>,
    _end: DateTime<Utc>,
) -> Result<Vec<ProvenanceRecord>, ProvenanceError> {
    Err(ProvenanceError::NotFound(
        "query_provenance_by_window is not implemented in this stub".to_string(),
    ))
}

// -----------------------------------------------------------------------------
// Kani harness stubs (module-level, #[cfg(kani)]).
// These are placeholders for real harnesses that will be written in a dedicated
// Kani crate or test module. They document the properties we intend to prove.
// -----------------------------------------------------------------------------

#[cfg(kani)]
mod kani_harnesses {
    use super::*;
    use rust_decimal::Decimal;

    fn dec(v: f32) -> Decimal {
        Decimal::from_f32(v).unwrap()
    }

    /// Property: building a provenance record with valid IDs never yields InvalidRecord.
    #[kani::proof]
    fn kani_build_provenance_record_valid_ids() {
        let ker = KerSnapshot {
            k: dec(0.95),
            e: dec(0.92),
            r: dec(0.08),
        };
        let roh = RohSnapshot {
            roh: dec(0.25),
            domain: ActionDomain::EcoRestoration,
            lane: ActionLane::Research,
        };
        let lyap = LyapunovResidualSnapshot {
            vcurrent: dec(1.0),
            vnext: dec(0.95),
            epsilon: dec(0.02),
        };
        let alnenvelope = AlnShardId {
            name: "ecosafety.corridor.v1".to_string(),
            version: "1.0.0".to_string(),
        };
        let source_meta = SourceMeta {
            source_id: "SRC-TEST".to_string(),
            source_kind: "test".to_string(),
            schema_version: "1.0.0".to_string(),
            content_hash_hex: "hash-test".to_string(),
        };

        let result = build_provenance_record(
            "DEC-1",
            "ACT-1",
            ActionDomain::EcoRestoration,
            ActionLane::Research,
            ker,
            roh,
            lyap,
            Some("TSAFE-1".to_string()),
            alnenvelope,
            GovernanceDecision::Allow,
            EuAiRiskLevel::High,
            source_meta,
        );

        assert!(result.is_ok());
    }

    /// Property: appending to Veritas-Chain with non-empty chain id yields Ok.
    #[kani::proof]
    fn kani_append_veritas_chain_non_empty_chain() {
        let ker = KerSnapshot {
            k: dec(0.90),
            e: dec(0.88),
            r: dec(0.10),
        };
        let roh = RohSnapshot {
            roh: dec(0.20),
            domain: ActionDomain::CityOperations,
            lane: ActionLane::Pilot,
        };
        let lyap = LyapunovResidualSnapshot {
            vcurrent: dec(1.0),
            vnext: dec(0.99),
            epsilon: dec(0.02),
        };
        let alnenvelope = AlnShardId {
            name: "city.ops.corridor.v1".to_string(),
            version: "1.0.1".to_string(),
        };
        let source_meta = SourceMeta {
            source_id: "SRC-KANI".to_string(),
            source_kind: "kani".to_string(),
            schema_version: "1.0.0".to_string(),
            content_hash_hex: "hash-kani".to_string(),
        };

        let record = build_provenance_record(
            "DEC-2",
            "ACT-2",
            ActionDomain::CityOperations,
            ActionLane::Pilot,
            ker,
            roh,
            lyap,
            None,
            alnenvelope,
            GovernanceDecision::Derate,
            EuAiRiskLevel::Limited,
            source_meta,
        )
        .unwrap();

        let anchor = append_veritas_chain(&record, "VERITAS-CHAIN-01");
        assert!(anchor.is_ok());
    }
}
