// filename: ppx_minimal_identity_kernel/src/ci_guard.rs
// repo: eco_restoration_shard/ppx_minimal_identity_kernel/src/ci_guard.rs

use regex::Regex;
use std::fs;
use std::path::Path;
use walkdir::Walkdir;

#[derive(Debug)]
pub struct CIGuardResult {
    pub ok: bool,
    pub violations: Vec<String>,
}

pub fn run_ci_guard(root: &Path) -> CIGuardResult {
    let mut violations = Vec::new();

    let banned_terms = [
        "LEGITIMATE",
        "ILLEGITIMATE",
        "VALID_DESCENDANT",
        "INVALID_DESCENDANT",
        "PERSONHOOD_SCORE",
        "IDENTITY_CLASSIFICATION",
    ];

    let re_ns = Regex::new(r"namespace\\s+PPX\\.IDENTITY\\.MINIMAL\\.CONTINUITY\\.NEURORIGHTS").unwrap();

    for entry in Walkdir::new(root).into_iter().filter_map(|e| e.ok()) {
        if !entry.file_type().is_file() {
            continue;
        }
        let path = entry.path();
        if let Some(ext) = path.extension() {
            if ext != "aln" && ext != "rs" && ext != "sql" {
                continue;
            }
        } else {
            continue;
        }

        let contents = match fs::read_to_string(path) {
            Ok(c) => c,
            Err(_) => continue,
        };

        if !re_ns.is_match(&contents) {
            continue;
        }

        for term in &banned_terms {
            if contents.contains(term) {
                violations.push(format!(
                    "BANNED_TERM '{}' found in file {}",
                    term,
                    path.display()
                ));
            }
        }
    }

    CIGuardResult {
        ok: violations.is_empty(),
        violations,
    }
}
