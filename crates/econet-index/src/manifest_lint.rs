// econet-index/src/manifest_lint.rs

use rusqlite::{params, Connection};
use std::path::Path;

pub struct RepoManifest {
    pub repo_name: String,
    pub github_slug: String,
    pub role_band: String,
    pub visibility: String,
    pub language_primary: String,
    pub description: Option<String>,
    pub ecosafety_binding: String,
    pub shard_protocol: String,
    pub lane_default: String,
    pub ker_target_k: f64,
    pub ker_target_e: f64,
    pub ker_target_r: f64,
    pub non_actuating_only: bool,
}

pub fn load_manifest(repo_root: &Path) -> anyhow::Result<RepoManifest> {
    let manifest_path = repo_root.join(".econet").join("econet_repo_index.sql");
    if !manifest_path.exists() {
        anyhow::bail!("Manifest file not found {:?}", manifest_path);
    }

    let conn = Connection::open_in_memory()?;
    let sql = std::fs::read_to_string(&manifest_path)?;
    conn.execute_batch(&sql)?;

    let mut stmt = conn.prepare(
        "SELECT repo_name, github_slug, role_band, visibility, language_primary,
                description, ecosafety_binding, shard_protocol, lane_default,
                ker_target_k, ker_target_e, ker_target_r, non_actuating_only
         FROM econet_repo_index",
    )?;

    let manifest = stmt.query_row(params![], |row| {
        Ok(RepoManifest {
            repo_name: row.get(0)?,
            github_slug: row.get(1)?,
            role_band: row.get(2)?,
            visibility: row.get(3)?,
            language_primary: row.get(4)?,
            description: row.get(5)?,
            ecosafety_binding: row.get(6)?,
            shard_protocol: row.get(7)?,
            lane_default: row.get(8)?,
            ker_target_k: row.get(9)?,
            ker_target_e: row.get(10)?,
            ker_target_r: row.get(11)?,
            non_actuating_only: row.get::<_, i64>(12)? != 0,
        })
    })?;

    Ok(manifest)
}

pub struct ManifestPolicy {
    pub allowed_role_bands: Vec<&'static str>,
    pub allowed_lanes: Vec<&'static str>,
    pub max_r_prod: f64,
    pub min_k_prod: f64,
    pub min_e_prod: f64,
}

impl Default for ManifestPolicy {
    fn default() -> Self {
        Self {
            allowed_role_bands: vec!["SPINE", "RESEARCH", "ENGINE", "MATERIAL", "GOV", "APP"],
            allowed_lanes: vec!["RESEARCH", "EXPPROD", "PROD"],
            max_r_prod: 0.13,
            min_k_prod: 0.90,
            min_e_prod: 0.90,
        }
    }
}

pub fn lint_manifest(manifest: &RepoManifest, policy: &ManifestPolicy) -> anyhow::Result<()> {
    if !policy
        .allowed_role_bands
        .iter()
        .any(|rb| *rb == manifest.role_band)
    {
        anyhow::bail!("Invalid role_band {}", manifest.role_band);
    }

    if !policy
        .allowed_lanes
        .iter()
        .any(|ln| *ln == manifest.lane_default)
    {
        anyhow::bail!("Invalid lane_default {}", manifest.lane_default);
    }

    if manifest.lane_default == "PROD" {
        if manifest.ker_target_k < policy.min_k_prod {
            anyhow::bail!(
                "ker_target_k too low for PROD ({} < {})",
                manifest.ker_target_k,
                policy.min_k_prod
            );
        }
        if manifest.ker_target_e < policy.min_e_prod {
            anyhow::bail!(
                "ker_target_e too low for PROD ({} < {})",
                manifest.ker_target_e,
                policy.min_e_prod
            );
        }
        if manifest.ker_target_r > policy.max_r_prod {
            anyhow::bail!(
                "ker_target_r too high for PROD ({} > {})",
                manifest.ker_target_r,
                policy.max_r_prod
            );
        }
    }

    Ok(())
}
