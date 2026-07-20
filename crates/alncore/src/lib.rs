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

pub use crate::parser::{
    AlnParseError,
    parse_aln_str,
    validate_aln_document,
    validate_deploy_kernel,
    validate_ker_snapshot,
    validate_safesteep_rule,
};

pub use crate::eval::{
    check_move,
    eval_deploy,
    eval_safesteep,
    explain_deploy,
    DeployDecision,
};

use serde_json::json;

// Hex-anchor DID binding + zk corridor verification crate.
// This keeps ALNv2 documents bound to governance DID and KER-safe.
use prometheus_praxis_hex_anchor::{
    KerPolicy,
    HexAnchorPublicInputs,
    HexAnchorVerificationResult,
    verify_hex_anchor_did_binding,
    GOVERNANCE_DID,
};

/// Serialize an AlnDocument to canonical JSON for cross-language conformance testing.
/// This JSON is the bridge format for C++ and other languages to consume ALNv2 contracts.
pub fn to_canonical_json(doc: &AlnDocument) -> String {
    let safesteprules: Vec<_> = doc
        .safesteprules
        .iter()
        .map(|r| {
            json!({
                "rule_id": r.rule_id,
                "description": r.description,
                "vt_ceiling": r.vt_ceiling,
                "epsilon": r.epsilon,
                "lyap_channel": r.lyap_channel,
            })
        })
        .collect();

    let deploy_kernels: Vec<_> = doc
        .deploy_kernels
        .iter()
        .map(|k| {
            json!({
                "kernel_id": k.kernel_id,
                "description": k.description,
                "k_min": k.k_min,
                "e_min": k.e_min,
                "r_max": k.r_max,
                "lane_scope": k.lane_scope,
            })
        })
        .collect();

    let override_policies: Vec<_> = doc
        .override_policies
        .iter()
        .map(|p| {
            json!({
                "policy_id": p.policy_id,
                "description": p.description,
                "allow_tighten_only": p.allow_tighten_only,
                "allow_emergency_derate": p.allow_emergency_derate,
                "forbid_safety_loosen": p.forbid_safety_loosen,
            })
        })
        .collect();

    let ker_snapshots: Vec<_> = doc
        .ker_snapshots
        .iter()
        .map(|s| {
            json!({
                "k": s.k,
                "e": s.e,
                "r": s.r,
                "vt": s.vt,
                "lane": match s.lane {
                    Lane::Research => "RESEARCH",
                    Lane::Exp => "EXP",
                    Lane::Sim => "SIM",
                    Lane::Prod => "PROD",
                },
                "completeness": match s.completeness {
                    KerCompleteness::Measured => "MEASURED",
                    KerCompleteness::Simulated => "SIMULATED",
                    KerCompleteness::Mixed => "MIXED",
                    KerCompleteness::Conceptual => "CONCEPTUAL",
                },
            })
        })
        .collect();

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
            "signing_hex": m.signing_hex,
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
        "ker_snapshots": ker_snapshots,
        "repo_manifest": repo_manifest,
    });

    serde_json::to_string_pretty(&json_obj).unwrap_or_else(|_| "{}".to_string())
}

/// High-level helper: parse an ALN string and return a validated document.
/// C++ can call this via extern "C" FFI using a wrapper that accepts UTF-8 bytes.
pub fn parse_and_validate_aln(input: &str) -> Result<AlnDocument, AlnParseError> {
    let doc = parse_aln_str(input)?;
    let errors = validate_aln_document(&doc);
    if errors.is_empty() {
        Ok(doc)
    } else {
        Err(AlnParseError {
            line: 1,
            message: format!("Validation errors: {}", errors.join("; ")),
        })
    }
}

/// Evaluate a single SafeStepRule against a previous and next residual value.
/// This is the Rust source-of-truth for ALNv2 safestep logic; C++ should mirror it.
pub fn eval_safestep_rule(
    rule: &SafeStepRule,
    vt_prev: f64,
    vt_next: f64,
) -> bool {
    eval_safesteep(rule, vt_prev, vt_next)
}

/// Evaluate all SafeStepRules in a document for a given residual step.
/// Returns true only if all rules admit the step.
pub fn eval_all_safestep_rules(
    doc: &AlnDocument,
    vt_prev: f64,
    vt_next: f64,
) -> bool {
    doc.safesteprules
        .iter()
        .all(|rule| eval_safesteep(rule, vt_prev, vt_next))
}

/// Evaluate deploy kernels against K,E,R and lane, returning the strongest decision.
/// C++ engines should match this behavior when implementing their own deploy gates.
pub fn eval_deploy_kernels(
    doc: &AlnDocument,
    k: f32,
    e: f32,
    r: f32,
    lane: Lane,
) -> DeployDecision {
    eval_deploy(&doc.deploy_kernels, k, e, r, lane)
}

/// Convenience: check a move using an AlnDocument and a KerSnapshot.
/// Intended as the canonical ALNv2 move gate; C++ should treat this as the reference.
pub fn check_move_with_snapshot(
    doc: &AlnDocument,
    snapshot: &KerSnapshot,
) -> DeployDecision {
    check_move(doc, snapshot)
}

/// FFI-friendly struct for exposing ALNv2 KER decisions to C++.
#[repr(C)]
pub struct AlnKerDecision {
    pub admissible: bool,
    pub lane: i32,
    pub code: i32,
}

/// FFI helper: given canonical JSON of an ALN document and KER values,
/// return an ALNv2 deploy decision in a C-compatible struct.
///
/// C++ wiring sketch:
/// - Use a thin wrapper that calls this function via extern "C".
/// - Map `lane_code` to your C++ Lane enum, and `code` to your deploy decision enum.
/// - Use the same canonical JSON generated by `to_canonical_json` on the Rust side.
pub fn ffi_eval_deploy_from_json(
    doc_json: &str,
    k: f32,
    e: f32,
    r: f32,
    lane_code: i32,
) -> AlnKerDecision {
    let lane = match lane_code {
        0 => Lane::Research,
        1 => Lane::Exp,
        2 => Lane::Sim,
        3 => Lane::Prod,
        _ => Lane::Research,
    };

    let parsed: serde_json::Value =
        serde_json::from_str(doc_json).unwrap_or_else(|_| serde_json::json!({}));

    let doc = AlnDocument::from_canonical_json(&parsed);
    let decision = eval_deploy_kernels(&doc, k, e, r, lane);

    AlnKerDecision {
        admissible: decision.admissible,
        lane: lane_code,
        code: decision.code as i32,
    }
}

/// Build a KerPolicy from an AlnDocument's RepoManifest.
/// This maps ALNv2 ker_target_* fields into the hex-anchor corridor policy.
pub fn ker_policy_from_repo_manifest(doc: &AlnDocument) -> Option<KerPolicy> {
    doc.repo_manifest.as_ref().map(|m| KerPolicy {
        k_min: m.ker_target_k,
        e_min: m.ker_target_e,
        r_max: m.ker_target_r,
        non_actuating: m.non_actuating_only,
    })
}

/// Verify that an AlnDocument is DID-bound and KER-safe using hex-anchor.
///
/// This function is non-actuating and intended for governance/audit flows.
/// `zk_proof_bytes` should come from the external zk prover asserting that
/// the hidden particle document obeys KerPolicy.
pub fn verify_document_hex_anchor(
    doc: &AlnDocument,
    zk_proof_bytes: &[u8],
) -> Result<HexAnchorVerificationResult, String> {
    let policy = ker_policy_from_repo_manifest(doc)
        .ok_or_else(|| "RepoManifest missing; cannot derive KerPolicy".to_string())?;

    // Map ALNv2 RepoManifest owner_did / signing_hex / evidence_hex to hex-anchor inputs.
    let manifest = doc
        .repo_manifest
        .as_ref()
        .ok_or_else(|| "RepoManifest missing; cannot verify DID binding".to_string())?;

    if manifest.owner_did != GOVERNANCE_DID {
        return Err(format!(
            "owner_did mismatch: expected {}, got {}",
            GOVERNANCE_DID, manifest.owner_did
        ));
    }

    let public_inputs = HexAnchorPublicInputs {
        did: manifest.owner_did.clone(),
        pubkey_hex: manifest.signing_hex.clone(),   // RepoManifest should store pk_gov in signing_hex
        evidencehex: manifest.evidence_hex.clone(), // hex commitment to the RepoManifest particle
        sig_hex: doc.signing_hex.clone(),           // signature over evidence_hex (document-level)
        policy,
    };

    verify_hex_anchor_did_binding(&public_inputs, zk_proof_bytes)
        .map_err(|e| format!("hex-anchor verification failed: {:?}", e))
}

/// Convenience: verify a RepoManifest hex-anchor binding directly.
/// This is useful for tools that work primarily at repo-level rather than document-level.
pub fn verify_repo_manifest_hex_anchor(
    manifest: &RepoManifest,
    zk_proof_bytes: &[u8],
    doc_signing_hex: &str,
) -> Result<HexAnchorVerificationResult, String> {
    if manifest.owner_did != GOVERNANCE_DID {
        return Err(format!(
            "owner_did mismatch: expected {}, got {}",
            GOVERNANCE_DID, manifest.owner_did
        ));
    }

    let policy = KerPolicy {
        k_min: manifest.ker_target_k,
        e_min: manifest.ker_target_e,
        r_max: manifest.ker_target_r,
        non_actuating: manifest.non_actuating_only,
    };

    let public_inputs = HexAnchorPublicInputs {
        did: manifest.owner_did.clone(),
        pubkey_hex: manifest.signing_hex.clone(),
        evidencehex: manifest.evidence_hex.clone(),
        sig_hex: doc_signing_hex.to_string(),
        policy,
    };

    verify_hex_anchor_did_binding(&public_inputs, zk_proof_bytes)
        .map_err(|e| format!("hex-anchor verification failed: {:?}", e))
}
