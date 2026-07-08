// filename: crates/alncore/src/parser.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

use crate::model::{
    AlnDocument, DeployDecisionKernel, KerCompleteness, Lane, OverridePolicy, RepoManifest,
    SafeStepRule,
};
use std::fmt;

#[derive(Debug)]
pub struct AlnParseError {
    pub line: usize,
    pub message: String,
}

impl fmt::Display for AlnParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "line {}: {}", self.line, self.message)
    }
}

impl std::error::Error for AlnParseError {}

fn parse_error(line: usize, msg: impl Into<String>) -> AlnParseError {
    AlnParseError {
        line,
        message: msg.into(),
    }
}

fn trim_comment(line: &str) -> &str {
    if let Some(idx) = line.find('#') {
        line[..idx].trim()
    } else if let Some(idx) = line.find("--") {
        line[..idx].trim()
    } else {
        line.trim()
    }
}

fn is_empty_or_comment(line: &str) -> bool {
    let trimmed = trim_comment(line);
    trimmed.is_empty()
}

fn parse_lane(s: &str) -> Option<Lane> {
    match s.to_uppercase().as_str() {
        "RESEARCH" => Some(Lane::Research),
        "EXP" => Some(Lane::Exp),
        "SIM" => Some(Lane::Sim),
        "PROD" => Some(Lane::Prod),
        _ => None,
    }
}

fn parse_ker_completeness(s: &str) -> Option<KerCompleteness> {
    match s.to_uppercase().as_str() {
        "MEASURED" => Some(KerCompleteness::Measured),
        "SIMULATED" => Some(KerCompleteness::Simulated),
        "MIXED" => Some(KerCompleteness::Mixed),
        "CONCEPTUAL" => Some(KerCompleteness::Conceptual),
        _ => None,
    }
}

fn parse_value_bool(s: &str) -> Option<bool> {
    match s.to_lowercase().as_str() {
        "true" | "yes" | "1" => Some(true),
        "false" | "no" | "0" => Some(false),
        _ => None,
    }
}

fn parse_value_f64(s: &str) -> Option<f64> {
    s.parse::<f64>().ok()
}

fn parse_value_f32(s: &str) -> Option<f32> {
    s.parse::<f32>().ok()
}

struct Tokenizer<'a> {
    lines: Vec<&'a str>,
    pos: usize,
}

impl<'a> Tokenizer<'a> {
    fn new(input: &'a str) -> Self {
        let lines: Vec<&str> = input.lines().collect();
        Tokenizer { lines, pos: 0 }
    }

    fn current_line_num(&self) -> usize {
        self.pos + 1
    }

    fn peek_non_empty(&mut self) -> Option<&'a str> {
        while self.pos < self.lines.len() {
            let line = self.lines[self.pos];
            if !is_empty_or_comment(line) {
                return Some(trim_comment(line));
            }
            self.pos += 1;
        }
        None
    }

    fn next_non_empty(&mut self) -> Option<&'a str> {
        let result = self.peek_non_empty();
        if result.is_some() {
            self.pos += 1;
        }
        result
    }

    fn expect_record_header(&mut self) -> Result<Option<&'a str>, AlnParseError> {
        if let Some(line) = self.next_non_empty() {
            if line.starts_with("record ") {
                Ok(Some(line.strip_prefix("record ").unwrap().trim()))
            } else if line == "endrecord" || line == "endsection" || line.starts_with("section ") {
                Ok(None)
            } else {
                Ok(None)
            }
        } else {
            Ok(None)
        }
    }
}

pub fn parse_aln_str(input: &str) -> Result<AlnDocument, AlnParseError> {
    let mut tok = Tokenizer::new(input);

    let mut doc_id = String::new();
    let mut schema_name = String::from("ALN-DOCUMENT");
    let mut version_tag = String::from("2026v1");
    let mut category = String::from("GENERAL");
    let mut region: Option<String> = None;
    let mut role_band = String::from("RESEARCH");
    let mut owner_did = String::new();
    let mut evidence_hex = String::new();
    let mut signing_hex = String::new();
    let mut parent_evidence_hex: Option<String> = None;

    let mut safesteprules: Vec<SafeStepRule> = Vec::new();
    let mut deploy_kernels: Vec<DeployDecisionKernel> = Vec::new();
    let mut override_policies: Vec<OverridePolicy> = Vec::new();
    let mut repo_manifest: Option<RepoManifest> = None;

    while let Some(line) = tok.next_non_empty() {
        // Check for document-level metadata
        if line.starts_with("docid ") || line.starts_with("doc_id ") {
            let val = line.split_whitespace().nth(1).unwrap_or("");
            doc_id = val.to_string();
            continue;
        }
        if line.starts_with("schemaname ") || line.starts_with("schema_name ") {
            let val = line.split_whitespace().nth(1).unwrap_or("");
            schema_name = val.to_string();
            continue;
        }
        if line.starts_with("versiontag ") || line.starts_with("version_tag ") {
            let val = line.split_whitespace().nth(1).unwrap_or("");
            version_tag = val.to_string();
            continue;
        }
        if line.starts_with("category ") {
            let val = line.split_whitespace().nth(1).unwrap_or("");
            category = val.to_string();
            continue;
        }
        if line.starts_with("region ") {
            let val = line.split_whitespace().nth(1).unwrap_or("");
            region = Some(val.to_string());
            continue;
        }
        if line.starts_with("roleband ") || line.starts_with("role_band ") {
            let val = line.split_whitespace().nth(1).unwrap_or("");
            role_band = val.to_string();
            continue;
        }
        if line.starts_with("ownerdid ") || line.starts_with("owner_did ") {
            let val = line.split_whitespace().nth(1).unwrap_or("");
            owner_did = val.to_string();
            continue;
        }
        if line.starts_with("evidencehex ") || line.starts_with("evidence_hex ") {
            let val = line.split_whitespace().nth(1).unwrap_or("");
            evidence_hex = val.to_string();
            continue;
        }
        if line.starts_with("signinghex ") || line.starts_with("signing_hex ") {
            let val = line.split_whitespace().nth(1).unwrap_or("");
            signing_hex = val.to_string();
            continue;
        }
        if line.starts_with("parentevidencehex ") || line.starts_with("parent_evidence_hex ") {
            let val = line.split_whitespace().nth(1).unwrap_or("");
            parent_evidence_hex = Some(val.to_string());
            continue;
        }

        // Handle record blocks
        if line.starts_with("record ") {
            let record_name = line.strip_prefix("record ").unwrap().trim();
            match record_name {
                "SafeStepRule2026v1" | "SafeStepRule" => {
                    let rule = parse_safestep_rule(&mut tok)?;
                    if let Some(r) = rule {
                        safesteprules.push(r);
                    }
                }
                "DeployDecisionKernel2026v1" | "DeployDecisionKernel" => {
                    let kernel = parse_deploy_kernel(&mut tok)?;
                    if let Some(k) = kernel {
                        deploy_kernels.push(k);
                    }
                }
                "KerSnapshot2026v1" | "KerSnapshot" => {
                    // For now, skip KerSnapshot records in top-level parsing
                    skip_record_block(&mut tok)?;
                }
                "RepoManifest2026v1" | "RepoManifest" => {
                    let manifest = parse_repo_manifest(&mut tok)?;
                    if let Some(m) = manifest {
                        repo_manifest = Some(m);
                    }
                }
                "OverridePolicy2026v1" | "OverridePolicy" => {
                    let policy = parse_override_policy(&mut tok)?;
                    if let Some(p) = policy {
                        override_policies.push(p);
                    }
                }
                "AlnDocument2026v1" | "AlnDocument" => {
                    // Parse inline document fields if present
                    parse_inline_document_fields(&mut tok, &mut doc_id, &mut schema_name, &mut version_tag,
                        &mut category, &mut region, &mut role_band, &mut owner_did,
                        &mut evidence_hex, &mut signing_hex, &mut parent_evidence_hex)?;
                }
                _ => {
                    // Unknown record - skip but could store in extras field later
                    skip_record_block(&mut tok)?;
                }
            }
        } else if line.starts_with("section ") {
            // Skip section blocks for now
            skip_section_block(&mut tok)?;
        }
    }

    // Build the document
    let mut doc = AlnDocument::new(
        doc_id,
        schema_name,
        version_tag,
        category,
        region,
        role_band,
        owner_did,
        evidence_hex,
        signing_hex,
        parent_evidence_hex,
    );

    for rule in safesteprules {
        doc.add_safesteprule(rule);
    }
    for kernel in deploy_kernels {
        doc.add_deploy_kernel(kernel);
    }
    doc.override_policies = override_policies;
    doc.repo_manifest = repo_manifest;

    // Validate the document
    let errors = validate_aln_document(&doc);
    if !errors.is_empty() {
        return Err(parse_error(1, format!("Validation errors: {}", errors.join("; "))));
    }

    Ok(doc)
}

fn skip_record_block(tok: &mut Tokenizer) -> Result<(), AlnParseError> {
    while let Some(line) = tok.next_non_empty() {
        if line == "endrecord" || line == "end" {
            return Ok(());
        }
    }
    Err(parse_error(tok.current_line_num(), "Unexpected end of input while parsing record"))
}

fn skip_section_block(tok: &mut Tokenizer) -> Result<(), AlnParseError> {
    while let Some(line) = tok.next_non_empty() {
        if line == "endsection" || line == "end" {
            return Ok(());
        }
    }
    Err(parse_error(tok.current_line_num(), "Unexpected end of input while parsing section"))
}

fn parse_inline_document_fields(
    tok: &mut Tokenizer,
    doc_id: &mut String,
    schema_name: &mut String,
    version_tag: &mut String,
    category: &mut String,
    region: &mut Option<String>,
    role_band: &mut String,
    owner_did: &mut String,
    evidence_hex: &mut String,
    signing_hex: &mut String,
    parent_evidence_hex: &mut Option<String>,
) -> Result<(), AlnParseError> {
    while let Some(line) = tok.next_non_empty() {
        if line == "endrecord" || line == "end" {
            return Ok(());
        }
        
        let parts: Vec<&str> = line.split_whitespace().collect();
        if parts.len() >= 2 {
            let key = parts[0].to_lowercase();
            let val = parts[1];
            match key.as_str() {
                "docid" | "doc_id" => *doc_id = val.to_string(),
                "schemaname" | "schema_name" => *schema_name = val.to_string(),
                "versiontag" | "version_tag" => *version_tag = val.to_string(),
                "category" => *category = val.to_string(),
                "region" => *region = Some(val.to_string()),
                "roleband" | "role_band" => *role_band = val.to_string(),
                "ownerdid" | "owner_did" => *owner_did = val.to_string(),
                "evidencehex" | "evidence_hex" => *evidence_hex = val.to_string(),
                "signinghex" | "signing_hex" => *signing_hex = val.to_string(),
                "parentevidencehex" | "parent_evidence_hex" => *parent_evidence_hex = Some(val.to_string()),
                _ => {}
            }
        }
    }
    Err(parse_error(tok.current_line_num(), "Unexpected end of input while parsing document fields"))
}

fn parse_safestep_rule(tok: &mut Tokenizer) -> Result<Option<SafeStepRule>, AlnParseError> {
    let mut rule_id = String::new();
    let mut description = String::new();
    let mut vt_ceiling: Option<f64> = None;
    let mut epsilon: f64 = 0.0;
    let mut lyap_channel = String::new();

    while let Some(line) = tok.next_non_empty() {
        if line == "endrecord" || line == "end" {
            break;
        }

        let parts: Vec<&str> = line.splitn(2, char::is_whitespace).collect();
        if parts.is_empty() {
            continue;
        }

        let key = parts[0].to_lowercase();
        let val = if parts.len() > 1 { parts[1].trim() } else { "" };

        match key.as_str() {
            "ruleid" | "rule_id" => rule_id = val.to_string(),
            "description" => description = val.to_string(),
            "vtceiling" | "vt_ceiling" => {
                vt_ceiling = parse_value_f64(val);
            }
            "epsilon" => {
                epsilon = parse_value_f64(val).unwrap_or(0.0);
            }
            "lyapchannel" | "lyap_channel" => lyap_channel = val.to_string(),
            _ => {}
        }
    }

    if rule_id.is_empty() {
        return Ok(None);
    }

    Ok(Some(SafeStepRule::new(rule_id, description, vt_ceiling, epsilon, lyap_channel)))
}

fn parse_deploy_kernel(tok: &mut Tokenizer) -> Result<Option<DeployDecisionKernel>, AlnParseError> {
    let mut kernel_id = String::new();
    let mut description = String::new();
    let mut k_min: f32 = 0.0;
    let mut e_min: f32 = 0.0;
    let mut r_max: f32 = 1.0;
    let mut lane_scope = String::new();

    while let Some(line) = tok.next_non_empty() {
        if line == "endrecord" || line == "end" {
            break;
        }

        let parts: Vec<&str> = line.splitn(2, char::is_whitespace).collect();
        if parts.is_empty() {
            continue;
        }

        let key = parts[0].to_lowercase();
        let val = if parts.len() > 1 { parts[1].trim() } else { "" };

        match key.as_str() {
            "kernelid" | "kernel_id" => kernel_id = val.to_string(),
            "description" => description = val.to_string(),
            "kmin" | "k_min" => {
                k_min = parse_value_f32(val).unwrap_or(0.0);
            }
            "emin" | "e_min" => {
                e_min = parse_value_f32(val).unwrap_or(0.0);
            }
            "rmax" | "r_max" => {
                r_max = parse_value_f32(val).unwrap_or(1.0);
            }
            "lanescope" | "lane_scope" => lane_scope = val.to_string(),
            _ => {}
        }
    }

    if kernel_id.is_empty() {
        return Ok(None);
    }

    Ok(Some(DeployDecisionKernel::new(kernel_id, description, k_min, e_min, r_max, lane_scope)))
}

fn parse_repo_manifest(tok: &mut Tokenizer) -> Result<Option<RepoManifest>, AlnParseError> {
    let mut repo_name = String::new();
    let mut github_slug = String::new();
    let mut role_band = String::from("RESEARCH");
    let mut lane_default = String::from("RESEARCH");
    let mut ecosafety_binding = String::new();
    let mut shard_protocol = String::new();
    let mut ker_target_k: f64 = 0.5;
    let mut ker_target_e: f64 = 0.5;
    let mut ker_target_r: f64 = 0.5;
    let mut non_actuating_only = false;
    let mut owner_did = String::new();
    let mut evidence_hex = String::new();
    let mut signing_hex = String::new();

    while let Some(line) = tok.next_non_empty() {
        if line == "endrecord" || line == "end" {
            break;
        }

        let parts: Vec<&str> = line.splitn(2, char::is_whitespace).collect();
        if parts.is_empty() {
            continue;
        }

        let key = parts[0].to_lowercase();
        let val = if parts.len() > 1 { parts[1].trim() } else { "" };

        match key.as_str() {
            "reponame" | "repo_name" => repo_name = val.to_string(),
            "githubslug" | "github_slug" => github_slug = val.to_string(),
            "roleband" | "role_band" => role_band = val.to_string(),
            "lanedefault" | "lane_default" => lane_default = val.to_string(),
            "ecosafetybinding" | "ecosafety_binding" => ecosafety_binding = val.to_string(),
            "shardprotocol" | "shard_protocol" => shard_protocol = val.to_string(),
            "kertargetk" | "ker_target_k" => {
                ker_target_k = parse_value_f64(val).unwrap_or(0.5);
            }
            "kertargete" | "ker_target_e" => {
                ker_target_e = parse_value_f64(val).unwrap_or(0.5);
            }
            "kertargetr" | "ker_target_r" => {
                ker_target_r = parse_value_f64(val).unwrap_or(0.5);
            }
            "nonactuatingonly" | "non_actuating_only" => {
                non_actuating_only = parse_value_bool(val).unwrap_or(false);
            }
            "ownerdid" | "owner_did" => owner_did = val.to_string(),
            "evidencehex" | "evidence_hex" => evidence_hex = val.to_string(),
            "signinghex" | "signing_hex" => signing_hex = val.to_string(),
            _ => {}
        }
    }

    if repo_name.is_empty() {
        return Ok(None);
    }

    Ok(Some(RepoManifest::new(
        repo_name,
        github_slug,
        role_band,
        lane_default,
        ecosafety_binding,
        shard_protocol,
        ker_target_k,
        ker_target_e,
        ker_target_r,
        non_actuating_only,
        owner_did,
        evidence_hex,
        signing_hex,
    )))
}

fn parse_override_policy(tok: &mut Tokenizer) -> Result<Option<OverridePolicy>, AlnParseError> {
    let mut policy_id = String::new();
    let mut description = String::new();
    let mut allow_tighten_only = false;
    let mut allow_emergency_derate = false;
    let mut forbid_safety_loosen = true;

    while let Some(line) = tok.next_non_empty() {
        if line == "endrecord" || line == "end" {
            break;
        }

        let parts: Vec<&str> = line.splitn(2, char::is_whitespace).collect();
        if parts.is_empty() {
            continue;
        }

        let key = parts[0].to_lowercase();
        let val = if parts.len() > 1 { parts[1].trim() } else { "" };

        match key.as_str() {
            "policyid" | "policy_id" => policy_id = val.to_string(),
            "description" => description = val.to_string(),
            "allowtightenonly" | "allow_tighten_only" => {
                allow_tighten_only = parse_value_bool(val).unwrap_or(false);
            }
            "allowemergencyderate" | "allow_emergency_derate" => {
                allow_emergency_derate = parse_value_bool(val).unwrap_or(false);
            }
            "forbidsafetyloosen" | "forbid_safety_loosen" => {
                forbid_safety_loosen = parse_value_bool(val).unwrap_or(true);
            }
            _ => {}
        }
    }

    if policy_id.is_empty() {
        return Ok(None);
    }

    Ok(Some(OverridePolicy::new(
        policy_id,
        description,
        allow_tighten_only,
        allow_emergency_derate,
        forbid_safety_loosen,
    )))
}

// Validation functions
pub fn validate_aln_document(doc: &AlnDocument) -> Vec<String> {
    let mut errors = Vec::new();

    // INV-ALNDOC-1: evidencehex/signinghex non-empty if roleband != "RESEARCH"
    if doc.role_band.to_uppercase() != "RESEARCH" {
        if doc.evidence_hex.is_empty() {
            errors.push("INV-ALNDOC-1: evidence_hex must be non-empty for non-RESEARCH roleband".to_string());
        }
        if doc.signing_hex.is_empty() {
            errors.push("INV-ALNDOC-1: signing_hex must be non-empty for non-RESEARCH roleband".to_string());
        }
    }

    // INV-REPO-1: for ecorestorationshard, roleband == "RESEARCH" and nonactuatingonly == true
    if let Some(ref manifest) = doc.repo_manifest {
        if manifest.github_slug.contains("ecorestorationshard") 
            || manifest.repo_name.contains("ecorestorationshard") {
            if manifest.role_band.to_uppercase() != "RESEARCH" {
                errors.push("INV-REPO-1: ecorestorationshard must have roleband == RESEARCH".to_string());
            }
            if !manifest.non_actuating_only {
                errors.push("INV-REPO-1: ecorestorationshard must have non_actuating_only == true".to_string());
            }
        }

        // INV-REPO-2: ownerdid in allowed Bostrom DID set
        let allowed_dids = [
            "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7",
            "bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc",
        ];
        if !allowed_dids.contains(&manifest.owner_did.as_str()) {
            errors.push(format!(
                "INV-REPO-2: owner_did '{}' not in allowed Bostrom DID set",
                manifest.owner_did
            ));
        }
    }

    // Validate SafeStepRules
    for rule in &doc.safesteprules {
        let rule_errors = validate_safestep_rule(rule);
        for err in rule_errors {
            errors.push(format!("SafeStepRule '{}': {}", rule.rule_id, err));
        }
    }

    // Validate DeployDecisionKernels
    for kernel in &doc.deploy_kernels {
        let kernel_errors = validate_deploy_kernel(kernel);
        for err in kernel_errors {
            errors.push(format!("DeployDecisionKernel '{}': {}", kernel.kernel_id, err));
        }
    }

    errors
}

pub fn validate_safestep_rule(rule: &SafeStepRule) -> Vec<String> {
    let mut errors = Vec::new();

    if rule.epsilon < 0.0 {
        errors.push("epsilon must be non-negative".to_string());
    }

    if let Some(ceil) = rule.vt_ceiling {
        if ceil < 0.0 {
            errors.push("vt_ceiling must be non-negative".to_string());
        }
    }

    if rule.rule_id.is_empty() {
        errors.push("rule_id must be non-empty".to_string());
    }

    if rule.lyap_channel.is_empty() {
        errors.push("lyap_channel must be non-empty".to_string());
    }

    errors
}

pub fn validate_deploy_kernel(kernel: &DeployDecisionKernel) -> Vec<String> {
    let mut errors = Vec::new();

    if kernel.k_min < 0.0 || kernel.k_min > 1.0 {
        errors.push(format!("k_min must be in range [0, 1], got {}", kernel.k_min));
    }

    if kernel.e_min < 0.0 || kernel.e_min > 1.0 {
        errors.push(format!("e_min must be in range [0, 1], got {}", kernel.e_min));
    }

    if kernel.r_max < 0.0 || kernel.r_max > 1.0 {
        errors.push(format!("r_max must be in range [0, 1], got {}", kernel.r_max));
    }

    if kernel.kernel_id.is_empty() {
        errors.push("kernel_id must be non-empty".to_string());
    }

    errors
}

pub fn validate_ker_snapshot(snapshot: &crate::model::KerSnapshot) -> Vec<String> {
    let mut errors = Vec::new();

    if snapshot.k < 0.0 || snapshot.k > 1.0 {
        errors.push(format!("k must be in range [0, 1], got {}", snapshot.k));
    }

    if snapshot.e < 0.0 || snapshot.e > 1.0 {
        errors.push(format!("e must be in range [0, 1], got {}", snapshot.e));
    }

    if snapshot.r < 0.0 || snapshot.r > 1.0 {
        errors.push(format!("r must be in range [0, 1], got {}", snapshot.r));
    }

    errors
}
