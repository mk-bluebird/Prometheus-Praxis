// filename: crates/alncore/src/lib.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

pub mod model;
pub mod parser;
pub mod eval;

pub use crate::model::{
    AlnDocument,
    DeployDecisionKernel,
    KerCompleteness,
    KerSnapshot,
    Lane,
    RepoManifest,
    SafeStepRule,
};

pub use crate::parser::{parse_aln_str, AlnParseError, validate_aln_document, validate_safestep_rule, validate_deploy_kernel, validate_ker_snapshot};
pub use crate::eval::{eval_safestep, eval_deploy, check_move, DeployDecision, explain_deploy};

/// Serialize an AlnDocument to canonical JSON for cross-language conformance testing.
pub fn to_canonical_json(doc: &AlnDocument) -> String {
    use serde_json::json;
    
    let safesteprules: Vec<_> = doc.safesteprules.iter().map(|r| {
        json!({
            "rule_id": r.rule_id,
            "description": r.description,
            "vt_ceiling": r.vt_ceiling,
            "epsilon": r.epsilon,
            "lyap_channel": r.lyap_channel
        })
    }).collect();
    
    let deploy_kernels: Vec<_> = doc.deploy_kernels.iter().map(|k| {
        json!({
            "kernel_id": k.kernel_id,
            "description": k.description,
            "k_min": k.k_min,
            "e_min": k.e_min,
            "r_max": k.r_max,
            "lane_scope": k.lane_scope
        })
    }).collect();
    
    let override_policies: Vec<_> = doc.override_policies.iter().map(|p| {
        json!({
            "policy_id": p.policy_id,
            "description": p.description,
            "allow_tighten_only": p.allow_tighten_only,
            "allow_emergency_derate": p.allow_emergency_derate,
            "forbid_safety_loosen": p.forbid_safety_loosen
        })
    }).collect();
    
    let repo_manifest = doc.repo_manifest.as_ref().map(|m| {
        json!({
            "repo_name": m.repo_name,
            "github_slug": m.github_slug,
            "role_band": m.role_band,
            "lane_default": m.lane_default,
            "ecosafety_binding": m.ecosafety_binding,
            "shard_protocol": m.shard_protocol,
            "ker_target_k": m.ker_target_k,
            "ker_target_e": m.ker_target_e,
            "ker_target_r": m.ker_target_r,
            "non_actuating_only": m.non_actuating_only,
            "owner_did": m.owner_did,
            "evidence_hex": m.evidence_hex,
            "signing_hex": m.signing_hex
        })
    });
    
    let json_obj = json!({
        "doc_id": doc.doc_id,
        "schema_name": doc.schema_name,
        "version_tag": doc.version_tag,
        "category": doc.category,
        "region": doc.region,
        "role_band": doc.role_band,
        "owner_did": doc.owner_did,
        "evidence_hex": doc.evidence_hex,
        "signing_hex": doc.signing_hex,
        "parent_evidence_hex": doc.parent_evidence_hex,
        "safesteprules": safesteprules,
        "deploy_kernels": deploy_kernels,
        "override_policies": override_policies,
        "repo_manifest": repo_manifest
    });
    
    serde_json::to_string_pretty(&json_obj).unwrap_or_else(|_| "{}".to_string())
}
