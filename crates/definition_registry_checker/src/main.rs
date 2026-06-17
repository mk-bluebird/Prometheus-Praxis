// Filename: crates/definition_registry_checker/src/main.rs
// Destination: crates/definition_registry_checker/src/main.rs

#![forbid(unsafe_code)]

use std::env;
use std::fs;
use std::path::{Path, PathBuf};

use rusqlite::{params, Connection};

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 5 {
        eprintln!("usage: definition_registry_checker --root <root> --definition-db <path>");
        std::process::exit(1);
    }

    let root = PathBuf::from(args[2].clone());
    let db_path = PathBuf::from(args[4].clone());

    let artifacts = scan_artifacts(&root);
    let conn = Connection::open(db_path).expect("open definition db");
    conn.execute("PRAGMA foreign_keys = ON", []).expect("pragma");

    for artifact in artifacts {
        check_artifact_registered(&conn, &artifact);
    }
}

#[derive(Debug)]
struct Artifact {
    rel_path: String,
    kind: String,
}

fn scan_artifacts(root: &Path) -> Vec<Artifact> {
    let mut artifacts = Vec::new();
    let mut stack = vec![root.to_path_buf()];
    while let Some(path) = stack.pop() {
        let entries = match fs::read_dir(&path) {
            Ok(e) => e,
            Err(_) => continue,
        };
        for entry in entries {
            if let Ok(entry) = entry {
                let p = entry.path();
                if p.is_dir() {
                    stack.push(p);
                    continue;
                }
                if let Some(ext) = p.extension() {
                    let ext_str = ext.to_string_lossy().to_lowercase();
                    if ext_str == "aln" || ext_str == "sql" || ext_str == "rs" {
                        if let Ok(rel) = p.strip_prefix(root) {
                            let rel_str = rel.to_string_lossy().replace('\\', "/");
                            let kind = match ext_str.as_str() {
                                "aln" => "ALN".to_string(),
                                "sql" => "SQL".to_string(),
                                "rs" => "RUST".to_string(),
                                _ => continue,
                            };
                            artifacts.push(Artifact {
                                rel_path: rel_str,
                                kind,
                            });
                        }
                    }
                }
            }
        }
    }
    artifacts
}

fn check_artifact_registered(conn: &Connection, artifact: &Artifact) {
    let mut stmt = conn
        .prepare(
            "SELECT COUNT(*)
             FROM grammarartifact
             WHERE repopath = ?1
               AND kind = ?2
               AND active = 1",
        )
        .expect("prepare query");
    let count: i64 = stmt
        .query_row(params![artifact.rel_path, artifact.kind], |row| row.get(0))
        .expect("query count");

    if count == 0 {
        eprintln!(
            "Artifact '{}' of kind '{}' missing from grammarartifact",
            artifact.rel_path, artifact.kind
        );
        std::process::exit(1);
    }
}
