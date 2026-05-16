// filename: src/kani/cargo_license_check.rs
// destination: eco_restoration_shard/src/kani/cargo_license_check.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

#![cfg(kani)]

use kani::any_string;

fn parse_license_field(toml_content: &str) -> Option<String> {
    for line in toml_content.lines() {
        let trimmed = line.trim();
        if trimmed.starts_with("license") {
            let parts: Vec<&str> = trimmed.split('=').collect();
            if parts.len() != 2 {
                continue;
            }
            let value = parts[1].trim();
            let cleaned = value.trim_matches('"').to_string();
            return Some(cleaned);
        }
    }
    None
}

#[kani::proof]
fn check_forbidden_license_absent() {
    let content = any_string();

    let license_opt = parse_license_field(&content);

    if let Some(license) = license_opt {
        assert!(!license.contains("Apache-2.0"));
        assert!(!license.contains("apache-2.0"));
        assert!(!license.contains("APACHE-2.0"));
    }
}
