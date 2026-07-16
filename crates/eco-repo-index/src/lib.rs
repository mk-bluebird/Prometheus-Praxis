use serde::Serialize;
use std::path::{Path, PathBuf};
use walkdir::WalkDir;

#[derive(Debug, Serialize)]
pub struct KerTargetSummary {
    pub k_min: f32,
    pub k_max: f32,
    pub e_min: f32,
    pub e_max: f32,
    pub r_min: f32,
    pub r_max: f32,
}

#[derive(Debug, Serialize)]
pub struct EcoRepoEntry {
    pub repo_id: String,
    pub aln_particle: String,
    pub ker_targets: Vec<KerTargetSummary>,
    pub lane: Option<String>,
    pub region: Option<String>,
    pub scope: Option<String>,
}

#[derive(Debug, Serialize)]
pub struct EcoRepoIndex {
    pub version: String,
    pub generated_at_utc: String,
    pub entries: Vec<EcoRepoEntry>,
}

pub fn discover_aln_particles(aln_root: &Path) -> Vec<EcoRepoEntry> {
    let mut entries = Vec::new();

    for entry in WalkDir::new(aln_root)
        .into_iter()
        .filter_map(Result::ok)
        .filter(|e| e.path().extension().and_then(|e| e.to_str()) == Some("aln"))
    {
        let path = entry.path();
        let content = match std::fs::read_to_string(path) {
            Ok(c) => c,
            Err(_) => continue,
        };

        // Simple particle extraction; more advanced parsing can reuse definition-registry logic.
        for line in content.lines() {
            let line = line.trim();
            if line.starts_with("particle ") {
                let name = line["particle ".len()..].split_whitespace().next().unwrap_or("").to_string();
                if name.is_empty() {
                    continue;
                }

                // Placeholder KER bounds; can be refined by parsing invariants.
                let ker_summary = KerTargetSummary {
                    k_min: 0.0,
                    k_max: 1.0,
                    e_min: 0.0,
                    e_max: 1.0,
                    r_min: 0.0,
                    r_max: 1.0,
                };

                entries.push(EcoRepoEntry {
                    repo_id: path.to_string_lossy().to_string(),
                    aln_particle: name,
                    ker_targets: vec![ker_summary],
                    lane: None,
                    region: None,
                    scope: None,
                });
            }
        }
    }

    entries
}
