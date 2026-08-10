// File: crates/aln-definition-registry/src/sql_aln_compliance.rs
use std::{
    collections::BTreeSet,
    fs,
    path::{Path, PathBuf},
};

#[derive(Debug, Eq, PartialEq)]
pub enum ComplianceError {
    Io(String),
    MissingSchemaBinding(PathBuf),
    MissingInvariant(PathBuf),
    MissingSqlConstraint { aln: PathBuf, constraint: String },
}

fn files_with_extension(root: &Path, extension: &str) -> Result<Vec<PathBuf>, ComplianceError> {
    let entries = fs::read_dir(root).map_err(|error| ComplianceError::Io(error.to_string()))?;
    let mut output = Vec::new();

    for entry in entries {
        let path = entry.map_err(|error| ComplianceError::Io(error.to_string()))?.path();
        if path.is_dir() {
            output.extend(files_with_extension(&path, extension)?);
        } else if path.extension().and_then(|value| value.to_str()) == Some(extension) {
            output.push(path);
        }
    }
    Ok(output)
}

fn normalize(expression: &str) -> String {
    expression
        .replace("frame.", "")
        .replace("K", "ker_k")
        .replace("E", "ker_e")
        .replace("R", "ker_r")
        .replace("deltaVt", "delta_vt")
        .replace(" ", "")
        .replace('\t', "")
        .replace('\r', "")
        .replace('\n', "")
        .to_ascii_lowercase()
}

fn bound_sql_path(aln_path: &Path, source: &str, repository_root: &Path) -> Option<PathBuf> {
    source.lines().find_map(|line| {
        line.trim()
            .strip_prefix("// sql-schema:")
            .map(str::trim)
            .map(|relative| repository_root.join(relative))
            .filter(|path| path.exists())
    }).or_else(|| {
        let _ = aln_path;
        None
    })
}

fn invariant_expressions(source: &str) -> BTreeSet<String> {
    let mut in_invariant = false;
    let mut expressions = BTreeSet::new();

    for raw in source.lines() {
        let line = raw.trim();
        if line.starts_with("invariant ") {
            in_invariant = true;
            continue;
        }
        if in_invariant && line == "}" {
            in_invariant = false;
            continue;
        }
        if in_invariant && !line.is_empty() && !line.starts_with("//") {
            expressions.insert(normalize(line));
        }
    }
    expressions
}

pub fn verify_repository(repository_root: &Path) -> Result<(), ComplianceError> {
    let aln_root = repository_root.join("aln");
    for aln_path in files_with_extension(&aln_root, "aln")? {
        let source = fs::read_to_string(&aln_path)
            .map_err(|error| ComplianceError::Io(error.to_string()))?;
        let sql_path = bound_sql_path(&aln_path, &source, repository_root)
            .ok_or_else(|| ComplianceError::MissingSchemaBinding(aln_path.clone()))?;
        let sql = fs::read_to_string(&sql_path)
            .map_err(|error| ComplianceError::Io(error.to_string()))?;
        let normalized_sql = normalize(&sql);
        let invariants = invariant_expressions(&source);

        if invariants.is_empty() {
            return Err(ComplianceError::MissingInvariant(aln_path));
        }
        for invariant in invariants {
            if !normalized_sql.contains(&invariant) {
                return Err(ComplianceError::MissingSqlConstraint {
                    aln: aln_path.clone(),
                    constraint: invariant,
                });
            }
        }
    }
    Ok(())
}
