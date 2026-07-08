// Example: Parse ecosafety ALN file and print safesteprules and deploy kernels

use alncore::{parse_aln_str, to_canonical_json};
use std::fs;

fn main() {
    // Read the ecosafety ALN file
    let aln_path = "ecosafety.dataqualityinvariants.v1.aln";
    
    let content = match fs::read_to_string(aln_path) {
        Ok(c) => c,
        Err(e) => {
            eprintln!("Failed to read {}: {}", aln_path, e);
            eprintln!("Usage: cargo run --example parse_ecosafety <path-to-aln-file>");
            return;
        }
    };

    println!("Parsing ALN file: {}\n", aln_path);
    println!("=== Raw Content (first 500 chars) ===\n");
    println!("{}", content.chars().take(500).collect::<String>());
    println!("\n...\n");

    match parse_aln_str(&content) {
        Ok(doc) => {
            println!("=== Parsed AlnDocument ===\n");
            println!("Doc ID: {}", doc.doc_id);
            println!("Schema: {}", doc.schema_name);
            println!("Version: {}", doc.version_tag);
            println!("Category: {}", doc.category);
            println!("Role Band: {}", doc.role_band);
            println!("Owner DID: {}", doc.owner_did);
            println!();
            
            println!("SafeStepRules count: {}", doc.safesteprules.len());
            for rule in &doc.safesteprules {
                println!("  - Rule: {} (epsilon={}, lyap_channel={})", 
                    rule.rule_id, rule.epsilon, rule.lyap_channel);
            }
            println!();
            
            println!("DeployDecisionKernels count: {}", doc.deploy_kernels.len());
            for kernel in &doc.deploy_kernels {
                println!("  - Kernel: {} (k_min={}, e_min={}, r_max={}, lane_scope={})", 
                    kernel.kernel_id, kernel.k_min, kernel.e_min, kernel.r_max, kernel.lane_scope);
            }
            println!();
            
            if let Some(ref manifest) = doc.repo_manifest {
                println!("Repo Manifest:");
                println!("  - Name: {}", manifest.repo_name);
                println!("  - GitHub: {}", manifest.github_slug);
                println!("  - Lane Default: {}", manifest.lane_default);
                println!();
            }
            
            println!("=== Canonical JSON ===\n");
            println!("{}", to_canonical_json(&doc));
        }
        Err(e) => {
            eprintln!("Parse error: {}", e);
        }
    }
}
