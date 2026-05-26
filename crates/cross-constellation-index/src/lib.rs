// filename: crates/cross-constellation-index/src/lib.rs
// destination: eco_restoration_shard/crates/cross-constellation-index/src/lib.rs

//! Cross-constellation index: discovers and normalizes repo manifests and
//! schema/index files across the EcoNet constellation.
//!
//! This is non-actuating logic: it only reads repo manifests and builds
//! an in-memory model that other tools (CI, agents, CLIs) can consume.

#![forbid(unsafe_code)]

use std::collections::BTreeMap;
use std::fs;
use std::path::{Path, PathBuf};

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use walkdir::WalkDir;

/// Logical role of a repository in the EcoNet constellation.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
pub enum RepoKind {
    EcoRestorationShard,
    EcoNet,
    DataLake,
    EcoFort,
    Cybercore,
    Other(String),
}

/// Minimal manifest for a repository, as used in existing DataLake and
/// ALN index JSON documents.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RepoManifest {
    pub repoid: String,
    pub version: String,
    pub description: Option<String>,
    pub sovereignty_lane: Option<String>,
    pub jurisdiction: Option<String>,
    pub path: PathBuf,
    pub kind: RepoKind,
}

/// A single schema or index entry discovered in the constellation.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SchemaEntry {
    pub schemaname: String,
    pub moduleid: Option<String>,
    pub filepath: PathBuf,
    pub language: Option<String>,
    pub kind: Option<String>,
    pub repoid: String,
}

/// Aggregated cross‑constellation index.
/// This is what AI agents and CI should use instead of guessing paths.
#[derive(Debug, Default, Clone, Serialize, Deserialize)]
pub struct CrossConstellationIndex {
    pub repos: BTreeMap<String, RepoManifest>,
    pub schemas: Vec<SchemaEntry>,
}

impl CrossConstellationIndex {
    /// Create an empty index.
    pub fn new() -> Self {
        Self {
            repos: BTreeMap::new(),
            schemas: Vec::new(),
        }
    }

    /// Load all repo manifests and schema index files under a given root.
    ///
    /// Typical `root` for this crate used from eco_restoration_shard CI:
    /// the workspace root that also contains the DataLake and EcoNet git checkouts.
    pub fn scan_root(root: &Path) -> Result<Self> {
        let mut index = CrossConstellationIndex::new();

        for entry in WalkDir::new(root)
            .follow_links(true)
            .into_iter()
            .filter_map(|e| e.ok())
        {
            let path = entry.path();

            if !path.is_file() {
                continue;
            }

            let file_name = match path.file_name().and_then(|s| s.to_str()) {
                Some(name) => name,
                None => continue,
            };

            // Repo manifest patterns.
            if file_name.ends_with(".repo.json") {
                let manifest = load_repo_manifest(path)?;
                index.repos.insert(manifest.repoid.clone(), manifest);
                continue;
            }

            // Schema index patterns (aligned with existing datalakealn-index.json, etc.).
            if file_name == "aln-index.json" || file_name.ends_with(".aln-index.json") {
                let repo_id = infer_repoid_from_path(path)
                    .unwrap_or_else(|| "unknown-repo".to_string());
                let entries = load_schema_entries(path, &repo_id)?;
                index.schemas.extend(entries);
            }
        }

        Ok(index)
    }

    /// Serialize the index to a JSON string for CLI or CI consumption.
    pub fn to_json(&self) -> Result<String> {
        let s = serde_json::to_string_pretty(self)?;
        Ok(s)
    }
}

/// Load a repo manifest JSON file (e.g. `datalakealn-data-lake.index.json` or
/// `datalakealn-pos-core.repo.json`-style documents).
fn load_repo_manifest(path: &Path) -> Result<RepoManifest> {
    let text = fs::read_to_string(path)
        .with_context(|| format!("reading repo manifest {:?}", path))?;

    // Allow a broad JSON structure but extract the fields we care about.
    let value: serde_json::Value = serde_json::from_str(&text)
        .with_context(|| format!("parsing repo manifest JSON {:?}", path))?;

    let repoid = value
        .get("repoid")
        .and_then(|v| v.as_str())
        .unwrap_or("unknown-repo")
        .to_string();

    let version = value
        .get("version")
        .and_then(|v| v.as_str())
        .unwrap_or("0.0.0")
        .to_string();

    let description = value
        .get("description")
        .and_then(|v| v.as_str())
        .map(|s| s.to_string());

    let sovereignty_lane = value
        .get("sovereignty")
        .and_then(|v| v.get("sovereigntylane"))
        .and_then(|v| v.as_str())
        .map(|s| s.to_string());

    let jurisdiction = value
        .get("sovereignty")
        .and_then(|v| v.get("jurisdiction"))
        .and_then(|v| v.as_str())
        .map(|s| s.to_string());

    let repo_kind = classify_repo_kind(&repoid, path);

    Ok(RepoManifest {
        repoid,
        version,
        description,
        sovereignty_lane,
        jurisdiction,
        path: path.to_path_buf(),
        kind: repo_kind,
    })
}

/// Load schema entries from an ALN index JSON, like `datalakealn-index.json`.
fn load_schema_entries(path: &Path, repoid: &str) -> Result<Vec<SchemaEntry>> {
    let text = fs::read_to_string(path)
        .with_context(|| format!("reading aln-index JSON {:?}", path))?;
    let value: serde_json::Value = serde_json::from_str(&text)
        .with_context(|| format!("parsing aln-index JSON {:?}", path))?;

    let mut out = Vec::new();

    let schemas = match value.get("schemas") {
        Some(s) => s,
        None => return Ok(out),
    };

    if let Some(array) = schemas.as_array() {
        for item in array {
            let schemaname = item
                .get("schemaname")
                .and_then(|v| v.as_str())
                .unwrap_or("unknown-schema")
                .to_string();

            let moduleid = item
                .get("moduleid")
                .and_then(|v| v.as_str())
                .map(|s| s.to_string());

            let filepath_str = item
                .get("filepath")
                .and_then(|v| v.as_str())
                .unwrap_or("");
            if filepath_str.is_empty() {
                continue;
            }

            let language = item
                .get("language")
                .and_then(|v| v.as_str())
                .map(|s| s.to_string());

            let kind = item
                .get("kind")
                .and_then(|v| v.as_str())
                .map(|s| s.to_string());

            let filepath = path
                .parent()
                .unwrap_or_else(|| Path::new("."))
                .join(filepath_str);

            out.push(SchemaEntry {
                schemaname,
                moduleid,
                filepath,
                language,
                kind,
                repoid: repoid.to_string(),
            });
        }
    }

    Ok(out)
}

/// Infer a repoid from a path when the JSON doesn't give one directly.
fn infer_repoid_from_path(path: &Path) -> Option<String> {
    // Heuristic: take the directory name one level above the file.
    path.parent()
        .and_then(|p| p.file_name())
        .and_then(|s| s.to_str())
        .map(|s| s.to_string())
}

/// Classify the repo kind using repoid or filesystem hints.
fn classify_repo_kind(repoid: &str, path: &Path) -> RepoKind {
    let repoid_lower = repoid.to_ascii_lowercase();

    if repoid_lower.contains("eco-restoration") || repoid_lower.contains("eco_restoration") {
        RepoKind::EcoRestorationShard
    } else if repoid_lower.contains("econet") {
        RepoKind::EcoNet
    } else if repoid_lower.contains("datalake") || repoid_lower.contains("data-lake") {
        RepoKind::DataLake
    } else if repoid_lower.contains("ecofort") || repoid_lower.contains("eco-fort") {
        RepoKind::EcoFort
    } else if repoid_lower.contains("cybercore") {
        RepoKind::Cybercore
    } else {
        // Fallback: try directory name.
        let dir = path
            .parent()
            .and_then(|p| p.file_name())
            .and_then(|s| s.to_str())
            .unwrap_or("")
            .to_ascii_lowercase();

        if dir.contains("eco_restoration") {
            RepoKind::EcoRestorationShard
        } else if dir.contains("econet") {
            RepoKind::EcoNet
        } else if dir.contains("datalake") {
            RepoKind::DataLake
        } else if dir.contains("eco-fort") || dir.contains("ecofort") {
            RepoKind::EcoFort
        } else if dir.contains("cybercore") {
            RepoKind::Cybercore
        } else {
            RepoKind::Other(repoid.to_string())
        }
    }
}
