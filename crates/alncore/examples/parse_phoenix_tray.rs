// Example: Parse Phoenix tray pilot ALN file

use alncore::{parse_aln_str, to_canonical_json};
use std::fs;

fn main() {
    // Read the Phoenix tray pilot ALN file
    let aln_path = "eco_restoration_shard/qpudatashards/LanePromotionReplayPolicyPhoenix2026v1.aln";
    
    let content = match fs::read_to_string(aln_path) {
        Ok(c) => c,
        Err(e) => {
            eprintln!("Failed to read {}: {}", aln_path, e);
            eprintln!("Note: This example expects to be run from the workspace root.");
            return;
        }
    };

    println!("Parsing Phoenix Tray Pilot ALN file: {}\n", aln_path);

    match parse_aln_str(&content) {
        Ok(doc) => {
            println!("=== Parsed AlnDocument ===\n");
            println!("Doc ID: {}", doc.doc_id);
            println!("Schema: {}", doc.schema_name);
            println!("Version: {}", doc.version_tag);
            println!("Category: {}", doc.category);
            println!("Role Band: {}", doc.role_band);
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
                println!("  - Non-actuating only: {}", manifest.non_actuating_only);
                println!();
            }
            
            println!("=== Canonical JSON (truncated) ===\n");
            let json = to_canonical_json(&doc);
            // Print first 2000 chars of JSON
            if json.len() > 2000 {
                println!("{}...\n[truncated]", &json[..2000]);
            } else {
                println!("{}", json);
            }
        }
        Err(e) => {
            eprintln!("Parse error: {}", e);
            eprintln!("\nNote: The Phoenix tray ALN uses a different syntax (particle/record/end format).");
            eprintln!("The parser may need updates to fully support this variant.");
        }
    }
}
