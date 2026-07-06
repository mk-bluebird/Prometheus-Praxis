// eco_restoration_shard/crates/prometheus_praxis_semantics/src/lib.rs
//
// Facade module for the Prometheus-Praxis Semantic Intent Translator.
// Re-exports the translator and semantic types for use by higher-level crates
// and MCP tool bindings. Non-actuating: no hardware, no network.

#![forbid(unsafe_code)]

mod semantic_intent_translator;

pub use semantic_intent_translator::{
    SemanticIntentTranslator,
    TaskKind,
    SemanticIntent,
    EnvelopeBindings,
    BoundedTargets,
    PlanStep,
    ExecutionPlan,
    SourceMeta,
    TrustVector,
    LexValidationResult,
    JanusVerificationResult,
    NyxSemantix,
    Semanticus,
    OntosVeritas,
    LexMachina,
    JanusVeritas,
};
