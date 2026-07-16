// crates/aln-definition-registry/src/lib.rs

use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};

use regex::Regex;
use walkdir::WalkDir;

/// Represents a single particle entry discovered in an ALN file.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParticleEntry {
    pub particle_name: String,
    pub aln_file_path: String,
    pub aln_title: Option<String>,
    pub aln_version: Option<String>,
    pub network: Option<String>,
    pub shardid: Option<String>,
    pub owner_bostrom: Option<String>,
    pub timestamp_utc: Option<String>,
}

/// Find all `.aln` files under the given root directory.
pub fn find_aln_files(root: &Path) -> Vec<PathBuf> {
    WalkDir::new(root)
        .into_iter()
        .filter_map(Result::ok)
        .filter(|e| e.path().extension().and_then(|e| e.to_str()) == Some("aln"))
        .map(|e| e.path().to_path_buf())
        .collect()
}

/// Parse high-level header fields from an ALN file.
pub fn parse_header_fields(content: &str) -> (Option<String>, Option<String>, Option<String>, Option<String>) {
    let mut title = None;
    let mut version = None;
    let mut network = None;
    let mut shardid = None;

    for line in content.lines() {
        let line = line.trim();
        if line.starts_with("title ") {
            title = Some(line["title ".len()..].trim().to_string());
        } else if line.starts_with("version ") {
            version = Some(line["version ".len()..].trim().to_string());
        } else if line.starts_with("network ") {
            network = Some(line["network ".len()..].trim().to_string());
        } else if line.starts_with("shardid ") {
            shardid = Some(line["shardid ".len()..].trim().to_string());
        }
    }

    (title, version, network, shardid)
}

/// Parse owner.bostrom and timestamp.utc fields from an ALN file.
pub fn parse_owner_timestamp(content: &str) -> (Option<String>, Option<String>) {
    let mut owner = None;
    let mut ts = None;

    for line in content.lines() {
        let line = line.trim();
        if line.starts_with("owner.bostrom ") {
            owner = Some(line["owner.bostrom ".len()..].trim().to_string());
        } else if line.starts_with("timestamp.utc ") {
            ts = Some(line["timestamp.utc ".len()..].trim().to_string());
        }
    }

    (owner, ts)
}

/// Extract all particle entries from a single ALN file.
pub fn extract_particles(path: &Path) -> Vec<ParticleEntry> {
    let content = match fs::read_to_string(path) {
        Ok(c) => c,
        Err(_) => return Vec::new(),
    };

    let (title, version, network, shardid) = parse_header_fields(&content);
    let (owner_bostrom, timestamp_utc) = parse_owner_timestamp(&content);

    let particle_re = Regex::new(r"(?m)^\s*particle\s+([A-Za-z0-9_]+)").unwrap();

    let mut entries = Vec::new();
    for cap in particle_re.captures_iter(&content) {
        let name = cap[1].to_string();
        entries.push(ParticleEntry {
            particle_name: name,
            aln_file_path: path.to_string_lossy().to_string(),
            aln_title: title.clone(),
            aln_version: version.clone(),
            network: network.clone(),
            shardid: shardid.clone(),
            owner_bostrom: owner_bostrom.clone(),
            timestamp_utc: timestamp_utc.clone(),
        });
    }

    entries
}

/// Load the existing registry template file.
///
/// The template must contain a `block registry_rows` with a `content`/`endcontent`
/// section that will be replaced by generated rows.
pub fn load_registry_template(path: &Path) -> String {
    fs::read_to_string(path).unwrap_or_else(|_| {
        panic!(
            "Failed to read registry template at {}",
            path.display()
        )
    })
}

/// Write the updated registry file by injecting generated rows into the
/// `registry_rows` content block.
///
/// The `entries` slice is expected to be sorted for deterministic output.
pub fn write_registry(path: &Path, template: &str, entries: &[ParticleEntry]) {
    let mut output = String::new();
    let mut in_block = false;
    let mut wrote_block = false;

    for line in template.lines() {
        let trimmed = line.trim();
        if trimmed.starts_with("block registry_rows") {
            in_block = true;
            wrote_block = false;
            output.push_str(line);
            output.push('\n');
            continue;
        }

        if in_block && trimmed.starts_with("content") && !wrote_block {
            // Inject new content block.
            output.push_str("  content\n");
            output.push_str("    ; particle_name\taln_file_path\taln_title\taln_version\tnetwork\tshardid\towner_bostrom\ttimestamp_utc\n");
            for e in entries {
                let row = format!(
                    "    {pn}\t{path}\t{title}\t{ver}\t{net}\t{shard}\t{owner}\t{ts}\n",
                    pn = e.particle_name,
                    path = e.aln_file_path,
                    title = e.aln_title.clone().unwrap_or_default(),
                    ver = e.aln_version.clone().unwrap_or_default(),
                    net = e.network.clone().unwrap_or_default(),
                    shard = e.shardid.clone().unwrap_or_default(),
                    owner = e.owner_bostrom.clone().unwrap_or_default(),
                    ts = e.timestamp_utc.clone().unwrap_or_default()
                );
                output.push_str(&row);
            }
            output.push_str("  endcontent\n");
            wrote_block = true;
            continue;
        }

        if in_block && trimmed.starts_with("endblock") {
            output.push_str(line);
            output.push('\n');
            in_block = false;
            continue;
        }

        if !in_block {
            output.push_str(line);
            output.push('\n');
        }
    }

    let mut file = fs::File::create(path).expect("Failed to write registry file");
    file.write_all(output.as_bytes())
        .expect("Failed to write registry content");
}

/// High-level helper to regenerate the `aln_particle_registry.aln` file.
///
/// - `repo_root` should point to the repository root.
/// - `aln_dir` is typically `repo_root.join("aln")`.
/// - `registry_file` is typically `aln_dir.join("aln_particle_registry.aln")`.
pub fn regenerate_registry(repo_root: &Path, aln_dir: &Path, registry_file: &Path) {
    let aln_files = find_aln_files(aln_dir);

    let mut entries = Vec::new();
    for path in aln_files {
        let mut ps = extract_particles(&path);
        entries.append(&mut ps);
    }

    // Stable ordering for deterministic diffs.
    entries.sort_by(|a, b| {
        a.particle_name
            .cmp(&b.particle_name)
            .then(a.aln_file_path.cmp(&b.aln_file_path))
    });

    let template = load_registry_template(registry_file);
    write_registry(registry_file, &template, &entries);
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use std::fs::File;
    use tempfile::tempdir;

    #[test]
    fn test_extract_particles_from_simple_aln() {
        let content = r#"
aln TestFile.aln
network ALN-MAINNET
shardid 0xtest
owner.bostrom bostrom18sd2
timestamp.utc 2026-07-16T00:00:00Z

title Test ALN
version 1

particle Foo2026v1
  field id string required true
endparticle

particle Bar2026v1
  field id string required true
endparticle
"#;

        let dir = tempdir().unwrap();
        let path = dir.path().join("TestFile.aln");
        let mut f = File::create(&path).unwrap();
        f.write_all(content.as_bytes()).unwrap();

        let entries = extract_particles(&path);
        assert_eq!(entries.len(), 2);
        assert_eq!(entries[0].particle_name, "Foo2026v1");
        assert_eq!(entries[1].particle_name, "Bar2026v1");
        assert_eq!(entries[0].aln_title.as_deref(), Some("Test ALN"));
    }

    #[test]
    fn test_write_registry_injects_rows() {
        let template = r#"
aln aln_particle_registry.aln
network ALN-MAINNET
shardid 0xregistry
owner.bostrom bostrom18sd2
timestamp.utc 2026-07-16T22:00:00Z

title ALN Particle Registry
version 1

particle AlnParticleRegistryRow2026v1
  field particle_name string required true
endparticle

block registry_rows
  language TEXT
  content
    ; placeholder
  endcontent
endblock
"#;

        let dir = tempdir().unwrap();
        let path = dir.path().join("aln_particle_registry.aln");

        let entries = vec![
            ParticleEntry {
                particle_name: "Foo2026v1".to_string(),
                aln_file_path: "aln/Foo.aln".to_string(),
                aln_title: Some("FooTitle".to_string()),
                aln_version: Some("1".to_string()),
                network: Some("ALN-MAINNET".to_string()),
                shardid: Some("0xfoo".to_string()),
                owner_bostrom: Some("bostrom18sd2".to_string()),
                timestamp_utc: Some("2026-07-16T00:00:00Z".to_string()),
            },
        ];

        write_registry(&path, template, &entries);

        let out = fs::read_to_string(&path).unwrap();
        assert!(out.contains("Foo2026v1"));
        assert!(out.contains("aln/Foo.aln"));
    }
}
