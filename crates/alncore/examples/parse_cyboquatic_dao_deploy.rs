// Example: Parse Cyboquatic DAO deploy ALN file

use alncore::{parse_aln_str, to_canonical_json};
use std::fs;

fn main() {
    // Try to find a Cyboquatic-related ALN file
    let aln_paths = [
        "ecosafety/cybo/CyboNodeEcoScore2026v1.aln",
        "knowledge/identity_bostrom_primary.aln",
    ];
    
    let mut found_path: Option<&str> = None;
    let mut content: Option<String> = None;
    
    for path in &aln_paths {
        if let Ok(c) = fs::read_to_string(path) {
            found_path = Some(path);
            content = Some(c);
            break;
        }
    }
    
    let (aln_path, content) = match (found_path, content) {
        (Some(p), Some(c)) => (p, c),
        _ => {
            eprintln!("Could not find any Cyboquatic-related ALN files.");
            eprintln!("Tried paths:");
            for p in &aln_paths {
                eprintln!("  - {}", p);
            }
            return;
        }
    };

    println!("Parsing Cyboquatic DAO Deploy ALN file: {}\n", aln_path);

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
                println!("  - Role Band: {}", manifest.role_band);
                println!();
            }
            
            println!("=== Canonical JSON ===\n");
            println!("{}", to_canonical_json(&doc));
        }
        Err(e) => {
            eprintln!("Parse error: {}", e);
            eprintln!("\nNote: This ALN file may use a different syntax variant.");
        }
    }
}
