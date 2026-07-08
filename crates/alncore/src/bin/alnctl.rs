// alnctl - ALN file validation and explanation CLI tool
// Usage:
//   alnctl validate PATH    - Validate an ALN file and print syntax + invariant errors
//   alnctl explain PATH     - Show human-readable explanation of safesteprule and deploydecisionkernel blocks

use alncore::{parse_aln_str, to_canonical_json, check_move, explain_deploy, DeployDecision};
use std::env;
use std::fs;

fn print_usage() {
    eprintln!("alnctl - ALN file validation and explanation tool");
    eprintln!();
    eprintln!("Usage:");
    eprintln!("  alnctl validate <PATH>   - Validate an ALN file and print syntax + invariant errors");
    eprintln!("  alnctl explain <PATH>    - Show human-readable explanation of safesteprule and deploydecisionkernel blocks");
    eprintln!("  alnctl json <PATH>       - Output canonical JSON representation");
    eprintln!();
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
            if doc.repo_manifest.is_some() {
                println!("  - RepoManifest: present");
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
    let content = fs::read_to_string(path)
        .map_err(|e| format!("Failed to read {}: {}", path, e))?;

    println!("Explaining ALN file: {}\n", path);

    match parse_aln_str(&content) {
        Ok(doc) => {
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

            // Show example explanations with sample snapshots
            if !doc.deploy_kernels.is_empty() {
                println!("=== Example Decision Explanations ===\n");
                
                use alncore::{KerSnapshot, KerCompleteness, Lane};
                
                // Create a sample snapshot for demonstration
                let sample_snapshot = KerSnapshot::new(
                    0.75,  // k
                    0.60,  // e  
                    0.30,  // r
                    0.05,  // vt
                    Lane::Research,
                    KerCompleteness::Measured,
                    false, // is_speculative
                );
                
                for kernel in &doc.deploy_kernels {
                    println!("{}", explain_deploy(&sample_snapshot, kernel));
                    println!();
                    
                    // Also demonstrate check_move with a sample rule
                    if let Some(rule) = doc.safesteprules.first() {
                        let decision = check_move(&sample_snapshot, 0.04, rule, kernel);
                        match decision {
                            DeployDecision::Admissible => {
                                println!("check_move result: ADMISSIBLE (with prev_vt=0.04)");
                            }
                            DeployDecision::Rejected { reason } => {
                                println!("check_move result: REJECTED - {}", reason);
                            }
                        }
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
        Err(e) => {
            eprintln!("✗ Parse Error: {}", e);
            Err(e.to_string())
        }
    }
}

fn cmd_json(path: &str) -> Result<(), String> {
    let content = fs::read_to_string(path)
        .map_err(|e| format!("Failed to read {}: {}", path, e))?;

    match parse_aln_str(&content) {
        Ok(doc) => {
            println!("{}", to_canonical_json(&doc));
            Ok(())
        }
        Err(e) => {
            eprintln!("✗ Parse Error: {}", e);
            Err(e.to_string())
        }
    }
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
        _ => {
            eprintln!("Unknown command: {}", command);
            print_usage();
            std::process::exit(1);
        }
    };

    if let Err(e) = result {
        std::process::exit(1);
    }
}
