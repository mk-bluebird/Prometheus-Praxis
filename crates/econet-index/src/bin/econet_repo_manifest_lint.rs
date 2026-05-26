// econet-index/src/bin/econet_repo_manifest_lint.rs

use econet_index::manifest_lint::{lint_manifest, load_manifest, ManifestPolicy};
use std::path::PathBuf;

fn main() {
    let repo_root = std::env::args()
        .nth(1)
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("."));

    let manifest = match load_manifest(&repo_root) {
        Ok(m) => m,
        Err(e) => {
            eprintln!("ERROR: failed to load manifest: {}", e);
            std::process::exit(1);
        }
    };

    let policy = ManifestPolicy::default();
    if let Err(e) = lint_manifest(&manifest, &policy) {
        eprintln!("ERROR: manifest lint failed: {}", e);
        std::process::exit(1);
    }

    println!("Manifest OK for repo {}", manifest.repo_name);
}
