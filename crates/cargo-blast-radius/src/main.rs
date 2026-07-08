// crates/cargo-blast-radius/src/main.rs
//
// cargo-blast-radius: offline, non-actuating crate impact visualizer for the
// Prometheus-Praxis mono-repo. It computes a "blast radius" over crates based
// on path dependencies and emits a plain-text report.
//
// License: MIT OR Apache-2.0

#![forbid(unsafe_code)]
#![cfg_attr(not(test), deny(warnings))]

use std::collections::{HashMap, HashSet, VecDeque};
use std::env;
use std::fs;
use std::io::{self, Read};
use std::path::{Path, PathBuf};

use serde::Deserialize;
use toml::Value as TomlValue;

/// High-level result for a blast-radius computation.
#[derive(Debug)]
struct BlastRadiusReport {
    root_crate: String,
    affected: Vec<String>,
    dependency_graph: HashMap<String, Vec<String>>,
}

/// Minimal representation of a Cargo.toml package section.
#[derive(Debug, Deserialize)]
struct PackageSection {
    name: String,
}

/// Minimal representation of a Cargo.toml dependencies section.
#[derive(Debug, Deserialize, Default)]
struct DependenciesSection {
    #[serde(flatten)]
    deps: HashMap<String, TomlValue>,
}

/// Minimal, partial Cargo.toml model used for blast radius.
///
/// This does not attempt to fully model Cargo metadata; it only extracts
/// the package name and direct (path) dependencies inside the workspace.
#[derive(Debug, Deserialize)]
struct CargoToml {
    package: Option<PackageSection>,

    #[serde(default)]
    dependencies: DependenciesSection,

    #[serde(rename = "dev-dependencies", default)]
    dev_dependencies: DependenciesSection,

    #[serde(rename = "build-dependencies", default)]
    build_dependencies: DependenciesSection,
}

/// CLI options.
#[derive(Debug)]
struct Cli {
    root_crate: String,
    workspace_root: PathBuf,
    json: bool,
}

fn main() {
    let cli = match parse_cli() {
        Ok(cli) => cli,
        Err(e) => {
            eprintln!("cargo-blast-radius: {e}");
            std::process::exit(1);
        }
    };

    let manifest_paths = match discover_crate_manifests(&cli.workspace_root) {
        Ok(paths) => paths,
        Err(e) => {
            eprintln!("cargo-blast-radius: failed to discover manifests: {e}");
            std::process::exit(1);
        }
    };

    let manifests = match load_manifests(&manifest_paths) {
        Ok(m) => m,
        Err(e) => {
            eprintln!("cargo-blast-radius: failed to load manifests: {e}");
            std::process::exit(1);
        }
    };

    let name_to_path = build_name_index(&manifests);

    if !name_to_path.contains_key(&cli.root_crate) {
        eprintln!(
            "cargo-blast-radius: root crate '{}' not found in workspace.",
            cli.root_crate
        );
        std::process::exit(1);
    }

    let graph = build_dependency_graph(&manifests, &name_to_path);
    let affected = compute_blast_radius(&graph, &cli.root_crate);

    let report = BlastRadiusReport {
        root_crate: cli.root_crate.clone(),
        affected,
        dependency_graph: graph,
    };

    if cli.json {
        if let Err(e) = emit_json(&report) {
            eprintln!("cargo-blast-radius: failed to emit json: {e}");
            std::process::exit(1);
        }
    } else if let Err(e) = emit_text(&report) {
        eprintln!("cargo-blast-radius: failed to emit report: {e}");
        std::process::exit(1);
    }
}

/// Parse CLI arguments.
///
/// Usage:
///   cargo-blast-radius <crate-name> [--workspace-root PATH] [--json]
fn parse_cli() -> Result<Cli, String> {
    let mut args = env::args().skip(1);
    let root_crate = match args.next() {
        Some(arg) if !arg.starts_with("--") => arg,
        _ => {
            return Err("usage: cargo-blast-radius <crate-name> [--workspace-root PATH] [--json]".to_string());
        }
    };

    let mut workspace_root = env::current_dir().map_err(|e| e.to_string())?;
    let mut json = false;

    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--workspace-root" => {
                let Some(path) = args.next() else {
                    return Err("--workspace-root requires a path argument".to_string());
                };
                workspace_root = PathBuf::from(path);
            }
            "--json" => {
                json = true;
            }
            other => {
                return Err(format!("unrecognized argument: {other}"));
            }
        }
    }

    Ok(Cli {
        root_crate,
        workspace_root,
        json,
    })
}

/// Discover all Cargo.toml files under the workspace root.
///
/// This intentionally limits depth and skips target/ and .git/ to avoid
/// scanning generated artifacts.
fn discover_crate_manifests(root: &Path) -> io::Result<Vec<PathBuf>> {
    let mut manifests = Vec::new();
    let mut queue = VecDeque::new();
    queue.push_back(root.to_path_buf());

    while let Some(dir) = queue.pop_front() {
        for entry in fs::read_dir(&dir)? {
            let entry = entry?;
            let path = entry.path();
            let file_name = entry.file_name();
            let file_name_str = file_name.to_string_lossy();

            if file_name_str == "target" || file_name_str == ".git" {
                continue;
            }

            if path.is_dir() {
                queue.push_back(path.clone());
                continue;
            }

            if file_name_str == "Cargo.toml" {
                manifests.push(path.clone());
            }
        }
    }

    Ok(manifests)
}

/// Load and parse all manifests.
fn load_manifests(paths: &[PathBuf]) -> io::Result<Vec<(PathBuf, CargoToml)>> {
    let mut result = Vec::with_capacity(paths.len());
    for p in paths {
        let mut file = fs::File::open(p)?;
        let mut buf = String::new();
        file.read_to_string(&mut buf)?;
        let parsed: CargoToml = toml::from_str(&buf).map_err(|e| {
            io::Error::new(
                io::ErrorKind::InvalidData,
                format!("failed to parse {}: {e}", p.display()),
            )
        })?;
        result.push((p.clone(), parsed));
    }
    Ok(result)
}

/// Build an index from crate name -> manifest path.
fn build_name_index(manifests: &[(PathBuf, CargoToml)]) -> HashMap<String, PathBuf> {
    let mut index = HashMap::new();
    for (path, manifest) in manifests {
        if let Some(pkg) = &manifest.package {
            index.insert(pkg.name.clone(), path.clone());
        }
    }
    index
}

/// Extract path-based dependencies from a manifest into crate-name dependencies.
///
/// Only path dependencies that resolve to known workspace crate names are used.
fn manifest_dependencies(
    manifest_path: &Path,
    manifest: &CargoToml,
    name_index: &HashMap<String, PathBuf>,
) -> Vec<String> {
    let mut names = HashSet::new();

    let root_dir = manifest_path
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .to_path_buf();

    for section in [
        &manifest.dependencies,
        &manifest.dev_dependencies,
        &manifest.build_dependencies,
    ] {
        for (dep_name, raw) in &section.deps {
            // Only consider tables with a "path" key; ignore version-only deps.
            if let TomlValue::Table(tbl) = raw {
                if let Some(TomlValue::String(path_str)) = tbl.get("path") {
                    let dep_manifest = root_dir.join(path_str).join("Cargo.toml");
                    // Try to match dep by name if it exists in the index.
                    if let Some(dep_crate_name) = name_index
                        .iter()
                        .find_map(|(name, p)| if *p == dep_manifest { Some(name) } else { None })
                    {
                        names.insert(dep_crate_name.clone());
                    } else {
                        // Fallback: if the dependency name itself is a known crate, use it.
                        if name_index.contains_key(dep_name) {
                            names.insert(dep_name.clone());
                        }
                    }
                }
            }
        }
    }

    names.into_iter().collect()
}

/// Build a simple dependency graph: crate name -> direct workspace dependencies.
fn build_dependency_graph(
    manifests: &[(PathBuf, CargoToml)],
    name_index: &HashMap<String, PathBuf>,
) -> HashMap<String, Vec<String>> {
    let mut graph: HashMap<String, Vec<String>> = HashMap::new();

    for (path, manifest) in manifests {
        if let Some(pkg) = &manifest.package {
            let deps = manifest_dependencies(path, manifest, name_index);
            graph.insert(pkg.name.clone(), deps);
        }
    }

    graph
}

/// Compute blast radius (all crates that depend directly or transitively on root_crate).
///
/// We invert the dependency graph (edges from dep -> dependents) and do a BFS starting
/// from the root crate.
fn compute_blast_radius(
    graph: &HashMap<String, Vec<String>>,
    root_crate: &str,
) -> Vec<String> {
    // Build reverse graph: dep -> dependents.
    let mut reverse: HashMap<&str, Vec<&str>> = HashMap::new();
    for (crate_name, deps) in graph {
        for dep in deps {
            reverse.entry(dep.as_str()).or_default().push(crate_name.as_str());
        }
    }

    let mut visited: HashSet<&str> = HashSet::new();
    let mut queue: VecDeque<&str> = VecDeque::new();

    queue.push_back(root_crate);
    visited.insert(root_crate);

    while let Some(current) = queue.pop_front() {
        if let Some(dependents) = reverse.get(current) {
            for &dep in dependents {
                if !visited.contains(dep) {
                    visited.insert(dep);
                    queue.push_back(dep);
                }
            }
        }
    }

    // Remove the root crate itself and sort.
    let mut out: Vec<String> = visited
        .into_iter()
        .filter(|name| *name != root_crate)
        .map(|s| s.to_string())
        .collect();
    out.sort();
    out
}

/// Emit report as human-readable text.
fn emit_text(report: &BlastRadiusReport) -> io::Result<()> {
    println!(
        "cargo-blast-radius: root crate '{}'",
        report.root_crate
    );
    println!();
    if report.affected.is_empty() {
        println!("No other workspace crates depend on this crate.");
        return Ok(());
    }

    println!("Crates affected by changes to '{}':", report.root_crate);
    for name in &report.affected {
        println!("  - {name}");
    }

    Ok(())
}

/// Emit report as JSON to stdout.
fn emit_json(report: &BlastRadiusReport) -> io::Result<()> {
    #[derive(serde::Serialize)]
    struct JsonReport<'a> {
        root_crate: &'a str,
        affected: &'a [String],
        dependency_graph: &'a HashMap<String, Vec<String>>,
    }

    let jr = JsonReport {
        root_crate: &report.root_crate,
        affected: &report.affected,
        dependency_graph: &report.dependency_graph,
    };

    let s = serde_json::to_string_pretty(&jr)
        .map_err(|e| io::Error::new(io::ErrorKind::Other, e.to_string()))?;
    println!("{s}");
    Ok(())
}
