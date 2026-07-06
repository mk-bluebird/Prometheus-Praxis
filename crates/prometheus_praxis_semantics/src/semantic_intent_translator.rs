// eco_restoration_shard/crates/prometheus_praxis_semantics/src/semantic_intent_translator.rs
//
// ROLE
//   Prometheus-Praxis Semantic Intent Translator.
//   Converts human / policy language into executable, ontology-safe task graphs,
//   then into MacroActionContext for the rights verification kernel.
//
// REQUIREMENTS
//   - Rust edition 2024, rust-version = "1.85".
//   - !forbid(unsafe_code).
//   - Non-actuating: no hardware or network calls.
//   - Acts as a semantic front-end; Nyx-Semantix / Semanticus / Ontos-Veritas / Lex Machina
//     / Janus-Veritas are mirrored as typed facades, not full implementations.

#![forbid(unsafe_code)]

use chrono::{DateTime, Utc};
use rust_decimal::Decimal;
use serde::{Deserialize, Serialize};

use prometheus_praxis::{
    ActionDomain,
    ActionLane,
    AlnShardId,
    AxiomEvaluation,
    KerSnapshot,
    LyapunovResidualSnapshot,
    MacroActionContext,
    PraxisGovernanceConfig,
    PraxisGovernanceKernel,
    ResPublicaEvaluation,
    RightsRiskSnapshot,
    RohSnapshot,
    ContractEnvelope,
};

/// High-level task kinds supported by Prometheus-Praxis.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum TaskKind {
    EcoRestoration,
    SmartCityUpgrade,
    HealthcareProcedure,
    AugmentationUpgrade,
    PaymentProgramRollout,
}

/// Parsed semantic intent from Nyx-Semantix.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SemanticIntent {
    pub task_kind: TaskKind,
    pub jurisdiction: String,
    pub service_class: String,
    pub goals: Vec<String>,
    pub ambiguity_flags: Vec<String>,
}

/// Canonical envelopes attached by Semanticus.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EnvelopeBindings {
    pub neurorights_envelope: AlnShardId,
    pub equality_envelope: AlnShardId,
    pub ecosafety_envelope: AlnShardId,
}

/// Bounded targets (Eco / RoH) derived from goals.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BoundedTargets {
    pub eco_target: Decimal,
    pub roh_target: Decimal,
}

/// A single step in an execution plan.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlanStep {
    pub step_id: String,
    pub description: String,
    pub domain: ActionDomain,
    pub device_class: String,
    pub time_window_start: DateTime<Utc>,
    pub time_window_end: DateTime<Utc>,
}

/// Execution plan produced by Semanticus.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExecutionPlan {
    pub plan_id: String,
    pub task_kind: TaskKind,
    pub jurisdiction: String,
    pub envelopes: EnvelopeBindings,
    pub bounded_targets: BoundedTargets,
    pub steps: Vec<PlanStep>,
    pub forbidden_fields_absent: bool,
}

/// Provenance meta for Ontos-Veritas.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SourceMeta {
    pub source_id: String,
    pub source_type: String,
    pub jurisdiction_ref: String,
    pub collector_id: String,
    pub content_hash_hex: String,
    pub schema_version: String,
    pub lineage_ids: Vec<String>,
    pub evidence_hex: String,
}

/// Trust vector returned by Ontos-Veritas.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TrustVector {
    pub completeness: Decimal,
    pub calibration: Decimal,
    pub independence: Decimal,
    pub above_threshold: bool,
}

/// Lex Machina validation result.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LexValidationResult {
    pub ok: bool,
    pub violation_count: u32,
}

/// Janus-Veritas verification result (integrity + factual correctness).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct JanusVerificationResult {
    pub integrity_ok: bool,
    pub factual_ok: bool,
    pub reasons: Vec<String>,
}

/// Facade for Nyx-Semantix classifier.
pub struct NyxSemantix;

impl NyxSemantix {
    pub fn parse_intent(nl: &str) -> SemanticIntent {
        // Stubbed, deterministic classifier; in practice, replaced by real engine.
        let kind = if nl.to_lowercase().contains("clinic") {
            TaskKind::HealthcareProcedure
        } else if nl.to_lowercase().contains("traffic") {
            TaskKind::SmartCityUpgrade
        } else if nl.to_lowercase().contains("forest")
            || nl.to_lowercase().contains("watershed")
        {
            TaskKind::EcoRestoration
        } else {
            TaskKind::PaymentProgramRollout
        };

        SemanticIntent {
            task_kind: kind,
            jurisdiction: "policy.jurisdiction.us-az-maricopa-phoenix.v1".to_string(),
            service_class: "default".to_string(),
            goals: vec![nl.to_string()],
            ambiguity_flags: Vec::new(),
        }
    }
}

/// Facade for Semanticus planner.
pub struct Semanticus;

impl Semanticus {
    pub fn bind_envelopes(intent: &SemanticIntent) -> EnvelopeBindings {
        EnvelopeBindings {
            neurorights_envelope: AlnShardId {
                name: "neurorights.envelope.citizen.v1".to_string(),
                version: "1.0.0".to_string(),
            },
            equality_envelope: AlnShardId {
                name: "equality.envelope.payment_and_access.v1".to_string(),
                version: "1.0.0".to_string(),
            },
            ecosafety_envelope: AlnShardId {
                name: "ecosafety.corridor.city.v1".to_string(),
                version: "1.0.0".to_string(),
            },
        }
    }

    pub fn derive_bounded_targets(intent: &SemanticIntent) -> BoundedTargets {
        // Clamp into [0,1] corridors; here deterministic placeholders.
        let eco_target = Decimal::from_f32(0.80).unwrap();
        let roh_target = Decimal::from_f32(0.20).unwrap();
        BoundedTargets { eco_target, roh_target }
    }

    pub fn build_plan(intent: &SemanticIntent) -> ExecutionPlan {
        let envelopes = Self::bind_envelopes(intent);
        let bounded_targets = Self::derive_bounded_targets(intent);
        let now = Utc::now();
        let later = now + chrono::Duration::hours(4);

        let domain = match intent.task_kind {
            TaskKind::EcoRestoration => ActionDomain::EcoRestoration,
            TaskKind::SmartCityUpgrade => ActionDomain::CityOperations,
            TaskKind::HealthcareProcedure => ActionDomain::MacroHealth,
            TaskKind::AugmentationUpgrade => ActionDomain::MacroHealth,
            TaskKind::PaymentProgramRollout => ActionDomain::CityOperations,
        };

        let step = PlanStep {
            step_id: "STEP-1".to_string(),
            description: "Canonical first step derived from Semanticus".to_string(),
            domain,
            device_class: "non-actuating-planner".to_string(),
            time_window_start: now,
            time_window_end: later,
        };

        ExecutionPlan {
            plan_id: format!("PLAN-{}", intent.task_kind as u8),
            task_kind: intent.task_kind.clone(),
            jurisdiction: intent.jurisdiction.clone(),
            envelopes,
            bounded_targets,
            steps: vec![step],
            forbidden_fields_absent: true,
        }
    }
}

/// Facade for Ontos-Veritas provenance checker.
pub struct OntosVeritas;

impl OntosVeritas {
    pub fn certify_source(meta: &SourceMeta) -> TrustVector {
        // Deterministic placeholder: require non-empty hashes and matching jurisdiction.
        let complete = !meta.content_hash_hex.is_empty() && !meta.schema_version.is_empty();
        let above = complete && meta.jurisdiction_ref.starts_with("policy.jurisdiction");
        TrustVector {
            completeness: if complete { Decimal::ONE } else { Decimal::ZERO },
            calibration: Decimal::from_f32(0.9).unwrap(),
            independence: Decimal::from_f32(0.7).unwrap(),
            above_threshold: above,
        }
    }
}

/// Facade for Lex Machina ontological and contract validation.
pub struct LexMachina;

impl LexMachina {
    pub fn validate_plan(_plan: &ExecutionPlan) -> LexValidationResult {
        // Stub: assume OK when forbidden_fields_absent is true.
        LexValidationResult {
            ok: _plan.forbidden_fields_absent,
            violation_count: if _plan.forbidden_fields_absent { 0 } else { 1 },
        }
    }
}

/// Facade for Janus-Veritas dual verification.
pub struct JanusVeritas;

impl JanusVeritas {
    pub fn verify(_plan: &ExecutionPlan, trust: &TrustVector) -> JanusVerificationResult {
        let integrity_ok = _plan.forbidden_fields_absent;
        let factual_ok = trust.above_threshold;
        let mut reasons = Vec::new();
        if !integrity_ok {
            reasons.push("Forbidden fields present in plan".to_string());
        }
        if !factual_ok {
            reasons.push("Trust vector below threshold".to_string());
        }
        JanusVerificationResult {
            integrity_ok,
            factual_ok,
            reasons,
        }
    }
}

/// Semantic translator facade.
///   - Parses natural language intent.
///   - Builds execution plan.
///   - Runs provenance / logical checks.
///   - Produces MacroActionContext for PraxisGovernanceKernel.
#[derive(Debug, Clone)]
pub struct SemanticIntentTranslator {
    kernel: PraxisGovernanceKernel,
}

impl SemanticIntentTranslator {
    pub fn new(config: PraxisGovernanceConfig) -> Self {
        let kernel = PraxisGovernanceKernel::new(config);
        Self { kernel }
    }

    /// Main entry point: translate NL and source meta to governance decision.
    pub fn translate_and_verify(
        &self,
        nl_intent: &str,
        source_meta: &SourceMeta,
        lane: ActionLane,
    ) -> (GovernanceDecision, QpuDataShardDescriptor, Option<AppealRecord>) {
        let intent = NyxSemantix::parse_intent(nl_intent);
        let plan = Semanticus::build_plan(&intent);

        let trust = OntosVeritas::certify_source(source_meta);
        let lex = LexMachina::validate_plan(&plan);
        let janus = JanusVeritas::verify(&plan, &trust);

        // Build AxiomEvaluation from Lex / Janus.
        let axioms = AxiomEvaluation {
            all_satisfied: lex.ok && janus.integrity_ok && janus.factual_ok,
            themis_violations: if lex.ok { 0 } else { 1 },
            pan_ethos_violations: if janus.factual_ok { 0 } else { 1 },
        };

        // Build RightsRiskSnapshot from trust vector.
        let rights_risk = RightsRiskSnapshot {
            q: Decimal::from_f32(0.10).unwrap(),     // placeholder scalar
            q_crit: Decimal::from_f32(0.30).unwrap(), // rights-risk ceiling
        };

        // Dummy KER / RoH derived from bounded targets.
        let ker = KerSnapshot {
            k: Decimal::from_f32(0.92).unwrap(),
            e: plan.bounded_targets.eco_target,
            r: Decimal::from_f32(0.10).unwrap(),
        };
        let roh = RohSnapshot {
            roh: plan.bounded_targets.roh_target,
            domain: Self::map_task_to_domain(intent.task_kind),
            lane,
        };

        let lyapunov = LyapunovResidualSnapshot {
            vcurrent: Decimal::from_f32(1.0).unwrap(),
            vnext: Decimal::from_f32(0.98).unwrap(),
            epsilon: Decimal::from_f32(0.03).unwrap(),
        };

        let alnenvelope = plan.envelopes.ecosafety_envelope.clone();

        let res_publica = Some(ResPublicaEvaluation {
            public_good_scalar: Decimal::from_f32(0.10).unwrap(),
            is_public_good: true,
        });

        let contract = None; // Corpus Juris contract binding can be added here.

        let ctx = MacroActionContext {
            action_id: format!("ACT-{}", plan.plan_id),
            domain: Self::map_task_to_domain(intent.task_kind),
            lane,
            ker,
            roh,
            lyapunov,
            alnenvelope,
            contract,
            axioms,
            res_publica,
            rights_risk,
        };

        self.kernel.verify_action(ctx)
    }

    fn map_task_to_domain(task: TaskKind) -> ActionDomain {
        match task {
            TaskKind::EcoRestoration => ActionDomain::EcoRestoration,
            TaskKind::SmartCityUpgrade => ActionDomain::CityOperations,
            TaskKind::HealthcareProcedure => ActionDomain::MacroHealth,
            TaskKind::AugmentationUpgrade => ActionDomain::MacroHealth,
            TaskKind::PaymentProgramRollout => ActionDomain::CityOperations,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn decimal(v: f32) -> Decimal {
        Decimal::from_f32(v).unwrap()
    }

    #[test]
    fn translator_allows_safe_intent() {
        let cfg = PraxisGovernanceConfig::default();
        let translator = SemanticIntentTranslator::new(cfg);

        let source_meta = SourceMeta {
            source_id: "SRC-1".to_string(),
            source_type: "Sensor".to_string(),
            jurisdiction_ref: "policy.jurisdiction.us-az-maricopa-phoenix.v1".to_string(),
            collector_id: "DID-COLLECTOR".to_string(),
            content_hash_hex: "abc123".to_string(),
            schema_version: "1.0.0".to_string(),
            lineage_ids: Vec::new(),
            evidence_hex: "evidence-hex".to_string(),
        };

        let (decision, _shard, appeal) = translator.translate_and_verify(
            "restore forest watershed in Phoenix district",
            &source_meta,
            ActionLane::Research,
        );

        assert_eq!(decision, GovernanceDecision::Allow);
        assert!(appeal.is_none());
    }

    #[test]
    fn translator_stops_when_trust_below_threshold() {
        let cfg = PraxisGovernanceConfig::default();
        let translator = SemanticIntentTranslator::new(cfg);

        let source_meta = SourceMeta {
            source_id: "SRC-2".to_string(),
            source_type: "Dataset".to_string(),
            jurisdiction_ref: "invalid.jurisdiction".to_string(),
            collector_id: "DID-COLLECTOR".to_string(),
            content_hash_hex: "".to_string(), // incomplete
            schema_version: "".to_string(),
            lineage_ids: Vec::new(),
            evidence_hex: "".to_string(),
        };

        let (decision, _shard, appeal) = translator.translate_and_verify(
            "traffic plan for downtown",
            &source_meta,
            ActionLane::Production,
        );

        assert_eq!(decision, GovernanceDecision::Stop);
        assert!(appeal.is_some());
    }
}
