// File: crates/aln-definition-registry/src/lib.rs
use std::{
    collections::BTreeSet,
    fs,
    io::Write,
    path::{Path, PathBuf},
};

pub mod sql_aln_compliance;

pub use sql_aln_compliance::{verify_repository, ComplianceError};

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

#[derive(Debug)]
pub enum RegistryError {
    Io(std::io::Error),
    MissingRegistryBlock,
    MissingContentBlock,
    InvalidRegistryTemplate,
}

impl std::fmt::Display for RegistryError {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Io(error) => write!(formatter, "filesystem error: {error}"),
            Self::MissingRegistryBlock => write!(formatter, "missing block registry_rows"),
            Self::MissingContentBlock => write!(formatter, "missing registry_rows content block"),
            Self::InvalidRegistryTemplate => write!(formatter, "invalid registry template structure"),
        }
    }
}

impl std::error::Error for RegistryError {}

impl From<std::io::Error> for RegistryError {
    fn from(error: std::io::Error) -> Self {
        Self::Io(error)
    }
}

fn is_aln_file(path: &Path) -> bool {
    matches!(
        path.extension().and_then(|extension| extension.to_str()),
        Some("aln") | Some("aln2")
    )
}

fn walk_aln_files(root: &Path, output: &mut Vec<PathBuf>) {
    let Ok(entries) = fs::read_dir(root) else {
        return;
    };

    for entry in entries.flatten() {
        let path = entry.path();
        if path.is_dir() {
            walk_aln_files(&path, output);
        } else if is_aln_file(&path) {
            output.push(path);
        }
    }
}

pub fn find_aln_files(root: &Path) -> Vec<PathBuf> {
    let mut files = Vec::new();
    walk_aln_files(root, &mut files);
    files.sort();
    files
}

fn value_after_prefix(line: &str, prefix: &str) -> Option<String> {
    line.strip_prefix(prefix)
        .map(str::trim)
        .filter(|value| !value.is_empty())
        .map(ToOwned::to_owned)
}

pub fn parse_header_fields(
    content: &str,
) -> (Option<String>, Option<String>, Option<String>, Option<String>) {
    let mut title = None;
    let mut version = None;
    let mut network = None;
    let mut shardid = None;

    for raw in content.lines() {
        let line = raw.trim();
        if title.is_none() {
            title = value_after_prefix(line, "title ")
                .or_else(|| value_after_prefix(line, "aln2-spec "));
        }
        if version.is_none() {
            version = value_after_prefix(line, "version ");
        }
        if network.is_none() {
            network = value_after_prefix(line, "network ");
        }
        if shardid.is_none() {
            shardid = value_after_prefix(line, "shardid ");
        }
    }

    (title, version, network, shardid)
}

pub fn parse_owner_timestamp(content: &str) -> (Option<String>, Option<String>) {
    let mut owner = None;
    let mut timestamp = None;

    for raw in content.lines() {
        let line = raw.trim();
        if owner.is_none() {
            owner = value_after_prefix(line, "owner.bostrom ")
                .or_else(|| value_after_prefix(line, "did-root primary "));
        }
        if timestamp.is_none() {
            timestamp = value_after_prefix(line, "timestamp.utc ");
        }
    }

    (owner, timestamp)
}

fn particle_name(line: &str) -> Option<&str> {
    let line = line.trim_start();
    let remainder = line.strip_prefix("particle ")?;
    let name = remainder
        .trim_start()
        .split(|character: char| character.is_whitespace() || character == '{')
        .next()?
        .trim();

    (!name.is_empty() && !name.starts_with("//") && !name.starts_with('#')).then_some(name)
}

pub fn extract_particles(path: &Path) -> Vec<ParticleEntry> {
    let Ok(content) = fs::read_to_string(path) else {
        return Vec::new();
    };

    let (title, version, network, shardid) = parse_header_fields(&content);
    let (owner_bostrom, timestamp_utc) = parse_owner_timestamp(&content);

    content
        .lines()
        .filter_map(particle_name)
        .map(|particle_name| ParticleEntry {
            particle_name: particle_name.to_owned(),
            aln_file_path: path.to_string_lossy().into_owned(),
            aln_title: title.clone(),
            aln_version: version.clone(),
            network: network.clone(),
            shardid: shardid.clone(),
            owner_bostrom: owner_bostrom.clone(),
            timestamp_utc: timestamp_utc.clone(),
        })
        .collect()
}

pub fn load_registry_template(path: &Path) -> Result<String, RegistryError> {
    Ok(fs::read_to_string(path)?)
}

fn field(value: Option<&str>) -> String {
    value
        .unwrap_or_default()
        .replace(['\t', '\n', '\r'], " ")
        .trim()
        .to_owned()
}

fn rendered_rows(entries: &[ParticleEntry]) -> String {
    let mut rows = String::from(
        "    ; particle_name\taln_file_path\taln_title\taln_version\tnetwork\tshardid\towner_bostrom\ttimestamp_utc\n",
    );

    for entry in entries {
        rows.push_str(&format!(
            "    {}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\n",
            field(Some(&entry.particle_name)),
            field(Some(&entry.aln_file_path)),
            field(entry.aln_title.as_deref()),
            field(entry.aln_version.as_deref()),
            field(entry.network.as_deref()),
            field(entry.shardid.as_deref()),
            field(entry.owner_bostrom.as_deref()),
            field(entry.timestamp_utc.as_deref()),
        ));
    }
    rows
}

pub fn render_registry(template: &str, entries: &[ParticleEntry]) -> Result<String, RegistryError> {
    let mut output = String::new();
    let mut in_registry_block = false;
    let mut in_content = false;
    let mut found_registry_block = false;
    let mut replaced_content = false;

    for line in template.lines() {
        let trimmed = line.trim();

        if !in_registry_block && trimmed.starts_with("block registry_rows") {
            if found_registry_block {
                return Err(RegistryError::InvalidRegistryTemplate);
            }
            found_registry_block = true;
            in_registry_block = true;
            output.push_str(line);
            output.push('\n');
            continue;
        }

        if in_registry_block && !in_content && trimmed == "content" {
            in_content = true;
            replaced_content = true;
            output.push_str("  content\n");
            output.push_str(&rendered_rows(entries));
            output.push_str("  endcontent\n");
            continue;
        }

        if in_registry_block && in_content {
            if trimmed == "endcontent" {
                in_content = false;
            }
            continue;
        }

        if in_registry_block && trimmed == "endblock" {
            if in_content {
                return Err(RegistryError::InvalidRegistryTemplate);
            }
            in_registry_block = false;
            output.push_str(line);
            output.push('\n');
            continue;
        }

        output.push_str(line);
        output.push('\n');
    }

    if !found_registry_block {
        return Err(RegistryError::MissingRegistryBlock);
    }
    if !replaced_content {
        return Err(RegistryError::MissingContentBlock);
    }
    if in_registry_block || in_content {
        return Err(RegistryError::InvalidRegistryTemplate);
    }

    Ok(output)
}

fn write_atomically(path: &Path, content: &str) -> Result<(), RegistryError> {
    let temporary = path.with_extension("aln.tmp");
    {
        let mut file = fs::File::create(&temporary)?;
        file.write_all(content.as_bytes())?;
        file.sync_all()?;
    }
    fs::rename(temporary, path)?;
    Ok(())
}

pub fn write_registry(
    path: &Path,
    template: &str,
    entries: &[ParticleEntry],
) -> Result<(), RegistryError> {
    write_atomically(path, &render_registry(template, entries)?)
}

fn repository_relative_path(repo_root: &Path, path: &Path) -> String {
    path.strip_prefix(repo_root)
        .unwrap_or(path)
        .to_string_lossy()
        .replace('\\', "/")
}

pub fn regenerate_registry(
    repo_root: &Path,
    aln_dir: &Path,
    registry_file: &Path,
) -> Result<usize, RegistryError> {
    let registry_canonical = registry_file.canonicalize().ok();
    let mut entries = Vec::new();

    for path in find_aln_files(aln_dir) {
        if registry_canonical.as_ref().is_some_and(|registry| {
            path.canonicalize().ok().as_ref() == Some(registry)
        }) {
            continue;
        }

        for mut entry in extract_particles(&path) {
            entry.aln_file_path = repository_relative_path(repo_root, &path);
            entries.push(entry);
        }
    }

    entries.sort_by(|left, right| {
        left.particle_name
            .cmp(&right.particle_name)
            .then(left.aln_file_path.cmp(&right.aln_file_path))
    });

    let mut deduplicated = BTreeSet::new();
    entries.retain(|entry| {
        deduplicated.insert((
            entry.particle_name.clone(),
            entry.aln_file_path.clone(),
        ))
    });

    let template = load_registry_template(registry_file)?;
    write_registry(registry_file, &template, &entries)?;
    Ok(entries.len())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use tempfile::tempdir;

    #[test]
    fn extracts_particles_from_legacy_and_aln_v2_syntax() {
        let content = r#"
aln2-spec canal-corridor
version 2
did-root primary bostrom18sd2

particle legacy_particle
endparticle

particle governance.canal.frame {
}
"#;

        let directory = tempdir().unwrap();
        let path = directory.path().join("canal.aln");
        fs::write(&path, content).unwrap();

        let entries = extract_particles(&path);
        assert_eq!(entries.len(), 2);
        assert_eq!(entries[0].particle_name, "legacy_particle");
        assert_eq!(entries[1].particle_name, "governance.canal.frame");
        assert_eq!(entries[0].aln_title.as_deref(), Some("canal-corridor"));
        assert_eq!(entries[0].owner_bostrom.as_deref(), Some("bostrom18sd2"));
    }

    #[test]
    fn render_preserves_registry_block_metadata() {
        let template = r#"
block registry_rows
  language TEXT
  content
    ; placeholder
  endcontent
endblock
"#;
        let entries = vec![ParticleEntry {
            particle_name: "workload_frame".into(),
            aln_file_path: "aln/cyboquatic/workload.aln".into(),
            aln_title: Some("workload".into()),
            aln_version: Some("2".into()),
            network: None,
            shardid: None,
            owner_bostrom: Some("bostrom18sd2".into()),
            timestamp_utc: None,
        }];

        let rendered = render_registry(template, &entries).unwrap();
        assert!(rendered.contains("language TEXT"));
        assert!(rendered.contains("workload_frame"));
        assert!(!rendered.contains("placeholder"));
    }

    #[test]
    fn regenerate_excludes_registry_source_file() {
        let directory = tempdir().unwrap();
        let aln_directory = directory.path().join("aln");
        fs::create_dir_all(&aln_directory).unwrap();

        let source = aln_directory.join("workload.aln");
        fs::write(&source, "particle workload_frame\n").unwrap();

        let registry = aln_directory.join("aln_particle_registry.aln");
        let mut file = fs::File::create(&registry).unwrap();
        file.write_all(
            b"block registry_rows\n  content\n  endcontent\nendblock\n",
        )
        .unwrap();

        let count = regenerate_registry(directory.path(), &aln_directory, &registry).unwrap();
        let output = fs::read_to_string(&registry).unwrap();

        assert_eq!(count, 1);
        assert!(output.contains("workload_frame"));
        assert!(!output.contains("aln_particle_registry\t"));
    }
}
