// filename: eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/crates/ai_plane_ci_guard/src/main.rs

use std::env;
use std::path::{Path, PathBuf};
use std::process::exit;

use rusqlite::{Connection, Row};
use serde::Serialize;
use thiserror::Error;
use walkdir::WalkDir;

#[derive(Debug, Error)]
enum GuardError {
    #[error("Missing required env var: {0}")]
    MissingEnv(&'static str),
    #[error("SQLite error: {0}")]
    Sqlite(#[from] rusqlite::Error),
    #[error("Filesystem error: {0}")]
    Fs(String),
    #[error("Validation error: {0}")]
    Validation(String),
}

#[derive(Debug, Serialize)]
struct Violation {
    filepath: String,
    reason: String,
}

#[derive(Debug, Serialize)]
struct GuardReport {
    anchor_logical_name: String,
    evidence_hex: String,
    violations: Vec<Violation>,
}

const ANCHOR_LOGICAL_NAME: &str = "PHX_AI_DC_GOV_FRAMEWORK_20260716";
const ANCHOR_EVIDENCE_HEX: &str = "0x20260716PHXAIDCGOVFRAMEWORKV1";

fn main() {
    if let Err(err) = run() {
        eprintln!(
            "ai_plane_ci_guard: failure: {err}\nHint: all AI-plane .aln/.sql files must be registered under {ANCHOR_LOGICAL_NAME} ({ANCHOR_EVIDENCE_HEX})."
        );
        exit(1);
    }
}

fn run() -> Result<(), GuardError> {
    let repo_root = env::var("REPO_ROOT").unwrap_or_else(|_| ".".to_string());
    let repo_root = PathBuf::from(repo_root);

    let db_path = env_var("PHOENIX_HEX_DB")?;
    let conn = Connection::open(&db_path)?;

    let anchor_id = lookup_anchor_id(&conn)?; // fails if anchor missing

    let files = collect_ai_plane_files(&repo_root)?;
    let registered = load_registered_files(&conn, anchor_id)?;

    let violations = validate_files(&files, &registered);

    let report = GuardReport {
        anchor_logical_name: ANCHOR_LOGICAL_NAME.to_string(),
        evidence_hex: ANCHOR_EVIDENCE_HEX.to_string(),
        violations,
    };

    if !report.violations.is_empty() {
        let json = serde_json::to_string_pretty(&report)
            .unwrap_or_else(|_| "{\"error\":\"failed to serialize report\"}".to_string());
        eprintln!("{json}");
        return Err(GuardError::Validation(format!(
            "found {} unregistered AI-plane file(s)",
            report.violations.len()
        )));
    }

    Ok(())
}

fn env_var(name: &'static str) -> Result<String, GuardError> {
    env::var(name).map_err(|_| GuardError::MissingEnv(name))
}

fn lookup_anchor_id(conn: &Connection) -> Result<i64, GuardError> {
    let mut stmt = conn.prepare(
        "SELECT anchor_id, evidencehex
         FROM phoenix_hex_anchor
         WHERE logical_name = ?1",
    )?;
    let mut rows = stmt.query([ANCHOR_LOGICAL_NAME])?;
    if let Some(row) = rows.next()? {
        let anchor_id: i64 = row.get(0)?;
        let evidencehex: String = row.get(1)?;
        if evidencehex != ANCHOR_EVIDENCE_HEX {
            return Err(GuardError::Validation(format!(
                "anchor {ANCHOR_LOGICAL_NAME} evidencehex mismatch: expected {ANCHOR_EVIDENCE_HEX}, got {evidencehex}"
            )));
        }
        Ok(anchor_id)
    } else {
        Err(GuardError::Validation(format!(
            "anchor {ANCHOR_LOGICAL_NAME} not found in phoenix_hex_anchor"
        )))
    }
}

fn collect_ai_plane_files(root: &Path) -> Result<Vec<PathBuf>, GuardError> {
    let ai_dir = root.join("eco_restoration_shard")
        .join("cyboquatic_progress")
        .join("ai_datacenter_governance");

    if !ai_dir.exists() {
        return Ok(Vec::new());
    }

    let mut files = Vec::new();
    for entry in WalkDir::new(&ai_dir)
        .follow_links(false)
        .into_iter()
        .filter_map(Result::ok)
        .filter(|e| e.file_type().is_file())
    {
        let path = entry.into_path();
        if let Some(ext) = path.extension().and_then(|s| s.to_str()) {
            if ext.eq_ignore_ascii_case("aln") || ext.eq_ignore_ascii_case("sql") {
                files.push(path);
            }
        }
    }
    Ok(files)
}

fn load_registered_files(conn: &Connection, anchor_id: i64) -> Result<Vec<String>, GuardError> {
    let mut stmt = conn.prepare(
        "SELECT relpath || '/' || filename AS fullpath
         FROM phoenix_hex_file
         WHERE anchor_id = ?1",
    )?;
    let rows = stmt.query_map([anchor_id], |row: &Row| row.get::<_, String>(0))?;
    let mut v = Vec::new();
    for r in rows {
        v.push(r?);
    }
    Ok(v)
}

fn normalize_repo_rel(root: &Path, file: &Path) -> Result<String, GuardError> {
    let abs_root = root
        .canonicalize()
        .map_err(|e| GuardError::Fs(format!("canonicalize repo_root failed: {e}")))?;
    let abs_file = file
        .canonicalize()
        .map_err(|e| GuardError::Fs(format!("canonicalize file failed: {e}")))?;

    let rel = abs_file
        .strip_prefix(&abs_root)
        .map_err(|e| GuardError::Fs(format!("strip_prefix failed: {e}")))?;
    Ok(rel.to_string_lossy().replace('\\', "/"))
}

fn validate_files(files: &[PathBuf], registered: &[String]) -> Vec<Violation> {
    let repo_root = env::var("REPO_ROOT").unwrap_or_else(|_| ".".to_string());
    let repo_root = PathBuf::from(repo_root);

    let mut violations = Vec::new();
    for f in files {
        let Ok(rel) = normalize_repo_rel(&repo_root, f) else {
            continue;
        };
        let is_registered = registered.iter().any(|r| r == &rel);
        if !is_registered {
            violations.push(Violation {
                filepath: rel,
                reason: format!(
                    "AI-plane file not registered under anchor {ANCHOR_LOGICAL_NAME} (evidencehex {ANCHOR_EVIDENCE_HEX}) in phoenix_hex_file"
                ),
            });
        }
    }
    violations
}
