// filename: src/bin/alnctl.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis
//
// alnctl - ALN file validation and explanation CLI tool
//
// Usage:
//   alnctl validate PATH           - Validate an ALN file and print syntax + invariant errors
//   alnctl explain PATH            - Show human-readable explanation of safesteprule and deploydecisionkernel blocks
//   alnctl json PATH               - Output canonical JSON representation
//   alnctl continuity PATH         - Inspect Prometheus-Praxis continuity proofs and global neurorights/equality guards
//   alnctl corridors PATH          - Inspect execution corridors (eco_restoration, healthcare_cybernetics, smartcity_payments)
//   alnctl metrics PATH            - Show required metrics and KER snapshot bands for a generated Rust crate

use alncore::{
    parse_aln_str,
    to_canonical_json,
    check_move_with_snapshot,
    explain_deploy,
    DeployDecision,
    AlnDocument,
    KerSnapshot,
    KerCompleteness,
    Lane,
};
use std::env;
use std::fs;

fn print_usage() {
    eprintln!("alnctl - ALN file validation and explanation tool");
    eprintln!();
    eprintln!("Usage:");
    eprintln!("  alnctl validate <PATH>    - Validate an ALN file and print syntax + invariant errors");
    eprintln!("  alnctl explain  <PATH>    - Show human-readable explanation of SafeStepRule and DeployDecisionKernel blocks");
    eprintln!("  alnctl json     <PATH>    - Output canonical JSON representation");
    eprintln!("  alnctl continuity <PATH>  - Inspect continuity proofs, neurorights, equality envelopes");
    eprintln!("  alnctl corridors  <PATH>  - Inspect execution corridors and planner constraints");
    eprintln!("  alnctl metrics    <PATH>  - Show required metrics and KER snapshot bands");
    eprintln!();
    eprintln!("Notes for C++/FFI:");
    eprintln!("  - Use `alncore::to_canonical_json` as the Rust -> C++ contract for ALNv2 documents.");
    eprintln!("  - C++ can mirror SafeStepRule and DeployDecisionKernel structs and replay decisions.");
    eprintln!();
}

fn read_doc(path: &str) -> Result<AlnDocument, String> {
    let content = fs::read_to_string(path)
        .map_err(|e| format!("Failed to read {}: {}", path, e))?;
    parse_aln_str(&content).map_err(|e| e.to_string())
}

fn cmd_validate(path: &str) -> Result<(), String> {
    let content = fs::read_to_string(path)
        .map_err(|e| format!("Failed to read {}: {}", path, e))?;

    println!("Validating ALN file: {}\n", path);

    match parse_aln_str(&content) {
        Ok(doc) => {
            println!("✓ Syntax: OK");
            println!("✓ Validation: OK");
            println!();
            println!("Document Summary:");
            println!("  - Doc ID: {}", doc.doc_id);
            println!("  - Schema: {}", doc.schema_name);
            println!("  - Version: {}", doc.version_tag);
            println!("  - Category: {}", doc.category);
            println!("  - Role Band: {}", doc.role_band);
            println!("  - SafeStepRules: {}", doc.safesteprules.len());
            println!("  - DeployDecisionKernels: {}", doc.deploy_kernels.len());
            println!("  - OverridePolicies: {}", doc.override_policies.len());
            println!("  - KerSnapshots: {}", doc.ker_snapshots.len());
            if let Some(manifest) = &doc.repo_manifest {
                println!("  - RepoManifest: {}", manifest.repo_name);
                println!("    - github_slug: {}", manifest.github_slug);
                println!("    - role_band: {}", manifest.role_band);
                println!("    - lane_default: {}", manifest.lane_default);
                println!("    - ker_target_k: {}", manifest.ker_target_k);
                println!("    - ker_target_e: {}", manifest.ker_target_e);
                println!("    - ker_target_r: {}", manifest.ker_target_r);
                println!("    - non_actuating_only: {}", manifest.non_actuating_only);
                println!("    - owner_did: {}", manifest.owner_did);
            } else {
                println!("  - RepoManifest: none");
            }
            Ok(())
        }
        Err(e) => {
            eprintln!("✗ Parse/Validation Error:");
            eprintln!("  {}", e);
            Err(e.to_string())
        }
    }
}

fn cmd_explain(path: &str) -> Result<(), String> {
    let doc = read_doc(path)?;

    println!("Explaining ALN file: {}\n", path);

    println!("=== SafeStepRules ===\n");
    for rule in &doc.safesteprules {
        println!("Rule: {}", rule.rule_id);
        println!("  Description: {}", rule.description);
        println!("  Epsilon (max step): {}", rule.epsilon);
        if let Some(ceil) = rule.vt_ceiling {
            println!("  Vt Ceiling: {}", ceil);
        }
        println!("  Lyapunov Channel: {}", rule.lyap_channel);
        println!();
    }

    println!("=== DeployDecisionKernels ===\n");
    for kernel in &doc.deploy_kernels {
        println!("Kernel: {}", kernel.kernel_id);
        println!("  Description: {}", kernel.description);
        println!("  K_min (knowledge): {}", kernel.k_min);
        println!("  E_min (eco-impact): {}", kernel.e_min);
        println!("  R_max (risk ceiling): {}", kernel.r_max);
        println!("  Lane Scope: {}", kernel.lane_scope);
        println!();
    }

    if !doc.deploy_kernels.is_empty() {
        println!("=== Example Decision Explanations ===\n");

        let sample_snapshot = KerSnapshot::new(
            0.75,                      // k
            0.60,                      // e
            0.30,                      // r
            0.05,                      // vt
            Lane::Research,
            KerCompleteness::Measured,
        );

        for kernel in &doc.deploy_kernels {
            println!("{}", explain_deploy(&sample_snapshot, kernel));
            println!();

            if let Some(rule) = doc.safesteprules.first() {
                let decision = check_move_with_snapshot(&doc, &sample_snapshot);
                match decision {
                    DeployDecision::Admissible => {
                        println!("check_move_with_snapshot: ADMISSIBLE for sample snapshot");
                    }
                    DeployDecision::Rejected { reason } => {
                        println!("check_move_with_snapshot: REJECTED - {}", reason);
                    }
                }
                println!("SafeStep rule applied: {} (epsilon={}, vt_ceiling={:?})",
                    rule.rule_id, rule.epsilon, rule.vt_ceiling);
                println!();
            }
        }
    }

    if doc.deploy_kernels.is_empty() && doc.safesteprules.is_empty() {
        println!("No SafeStepRules or DeployDecisionKernels found in this document.");
        println!("This may be a metadata-only ALN file or use a different schema variant.");
    }

    Ok(())
}

fn cmd_json(path: &str) -> Result<(), String> {
    let doc = read_doc(path)?;
    println!("{}", to_canonical_json(&doc));
    Ok(())
}

/// Continuity-focused inspection:
/// surfaces Prometheus-Praxis continuity proof structures, neurorights, equality envelopes,
/// and global ROH ceilings if present in the ALN shard.
fn cmd_continuity(path: &str) -> Result<(), String> {
    let content = fs::read_to_string(path)
        .map_err(|e| format!("Failed to read {}: {}", path, e))?;

    println!("Continuity and neurorights inspection for ALN file: {}\n", path);

    let mut has_continuity_proof = false;
    let mut roh_global_ceiling: Option<String> = None;
    let mut monotone_ota: Option<String> = None;
    let mut no_actuation_fields: Option<String> = None;
    let mut neurorights_env: Option<String> = None;
    let mut equality_env: Option<String> = None;

    for line in content.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with(";;") {
            continue;
        }

        if line.starts_with("prometheuspraxis.continuity_proof.type") {
            has_continuity_proof = true;
        }
        if line.starts_with("invariant.roh_global_ceiling") {
            roh_global_ceiling = line.split_whitespace().nth(1).map(|s| s.to_string());
        }
        if line.starts_with("invariant.monotone_ota") {
            monotone_ota = line.split_whitespace().nth(1).map(|s| s.to_string());
        }
        if line.starts_with("invariant.no_actuation_fields") {
            no_actuation_fields = line.split_whitespace().nth(1).map(|s| s.to_string());
        }
        if line.starts_with("ref.neurorights_envelope") {
            neurorights_env = line.split_whitespace().nth(1).map(|s| s.to_string());
        }
        if line.starts_with("ref.equality_envelope") {
            equality_env = line.split_whitespace().nth(1).map(|s| s.to_string());
        }
    }

    if has_continuity_proof {
        println!("✓ Prometheus-Praxis continuity_proof type present.");
    } else {
        println!("⚠ No Prometheus-Praxis continuity_proof type found.");
    }

    if let Some(roh) = roh_global_ceiling {
        println!("Global ROH ceiling invariant: roh_global_ceiling = {}", roh);
    } else {
        println!("No roh_global_ceiling invariant found.");
    }

    if let Some(mon) = monotone_ota {
        println!("Monotone OTA invariant: monotone_ota = {}", mon);
    } else {
        println!("No monotone_ota invariant found.");
    }

    if let Some(fields) = no_actuation_fields {
        println!("Non-actuation fields invariant: no_actuation_fields = {}", fields);
    } else {
        println!("No no_actuation_fields invariant found.");
    }

    if let Some(n_env) = neurorights_env {
        println!("Neurorights envelope ref: {}", n_env);
    }
    if let Some(e_env) = equality_env {
        println!("Equality envelope ref: {}", e_env);
    }

    println!();
    println!("Use this output to ensure any Rust/CPP kernels stay within the neurorights and equality envelopes.");
    Ok(())
}

/// Corridor and planner inspection:
/// surfaces eco_restoration, healthcare_cybernetics, smartcity_payments corridors,
/// and planner_constraints from a Prometheus-Praxis ALN shard.
fn cmd_corridors(path: &str) -> Result<(), String> {
    let content = fs::read_to_string(path)
        .map_err(|e| format!("Failed to read {}: {}", path, e))?;

    println!("Execution corridors and planner constraints for ALN file: {}\n", path);

    let mut current_corridor: Option<String> = None;

    for line in content.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with(";;") {
            continue;
        }

        if line.starts_with("corridor.") {
            current_corridor = Some(line.to_string());
            println!("{}", line);
            continue;
        }

        if let Some(ref name) = current_corridor {
            if line.starts_with("corridor.") {
                current_corridor = Some(line.to_string());
                println!("\n{}", line);
                continue;
            }
            println!("  {}", line);
        }

        if line.starts_with("prometheuspraxis.planner_constraints") {
            println!("\nPlanner constraints:");
        }

        if line.starts_with("prometheuspraxis.planner_constraints") {
            println!("  {}", line);
        } else if line.starts_with("forbidden.fields")
            || line.starts_with("inequality.")
            || line.starts_with("require.")
        {
            println!("  {}", line);
        }
    }

    println!();
    println!("Use corridor and planner information to align Rust/CPP controllers with ALNv2 execution envelopes.");
    Ok(())
}

/// Metrics inspection:
/// surfaces metric.required entries and KER-related expectations from a Prometheus-Praxis shard.
fn cmd_metrics(path: &str) -> Result<(), String> {
    let content = fs::read_to_string(path)
        .map_err(|e| format!("Failed to read {}: {}", path, e))?;

    println!("Required metrics and KER expectations for ALN file: {}\n", path);

    let mut metrics: Vec<String> = Vec::new();

    for line in content.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with(";;") {
            continue;
        }

        if line.starts_with("metric.required") {
            metrics.push(line.to_string());
        }
    }

    if metrics.is_empty() {
        println!("No metric.required entries found.");
    } else {
        println!("Required metrics:");
        for m in metrics {
            println!("  {}", m);
        }
    }

    println!();
    println!("Rust and C++ crates that implement Prometheus-Praxis kernels should expose these metrics as first-class outputs.");
    Ok(())
}

fn main() {
    let args: Vec<String> = env::args().collect();

    if args.len() < 3 {
        print_usage();
        std::process::exit(1);
    }

    let command = &args[1];
    let path = &args[2];

    let result = match command.as_str() {
        "validate" => cmd_validate(path),
        "explain" => cmd_explain(path),
        "json" => cmd_json(path),
        "continuity" => cmd_continuity(path),
        "corridors" => cmd_corridors(path),
        "metrics" => cmd_metrics(path),
        _ => {
            eprintln!("Unknown command: {}", command);
            print_usage();
            std::process::exit(1);
        }
    };

    if let Err(e) = result {
        eprintln!("Error: {}", e);
        std::process::exit(1);
    }
}
