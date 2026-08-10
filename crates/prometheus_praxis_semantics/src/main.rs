// File: prometheus_praxis_semantics/src/main.rs
use std::{env, fs, path::Path};

fn escape(value: &str) -> String {
    value.replace('\\', "\\\\").replace('"', "\\\"")
}

fn scan(path: &Path, entries: &mut Vec<String>) {
    let Ok(read_dir) = fs::read_dir(path) else { return };
    for entry in read_dir.flatten() {
        let path = entry.path();
        if path.is_dir() {
            scan(&path, entries);
            continue;
        }
        let supported = ["rs", "cpp", "h", "java", "kt", "lua", "aln"];
        if !path.extension().and_then(|x| x.to_str()).is_some_and(|x| supported.contains(&x)) {
            continue;
        }
        let Ok(source) = fs::read_to_string(&path) else { continue };
        let mut invariants = Vec::new();
        for line in source.lines().map(str::trim) {
            if line.starts_with("invariant ") || line.starts_with("CHECK (") {
                invariants.push(escape(line));
            }
            let marker = ["pub fn ", "public static ", "public ", "fun ", "function M."];
            if let Some(prefix) = marker.iter().find(|prefix| line.starts_with(**prefix)) {
                let signature = line.trim_start_matches(*prefix)
                    .split('{').next().unwrap_or(line).trim();
                entries.push(format!(
                    "{{\"file\":\"{}\",\"function\":\"{}\",\"parameters\":\"{}\",\"constraints\":[]}}",
                    escape(&path.display().to_string()),
                    escape(signature.split('(').next().unwrap_or(signature)),
                    escape(signature.split_once('(').map(|x| x.1.trim_end_matches(')')).unwrap_or(""))
                ));
            }
        }
        if !invariants.is_empty() {
            entries.push(format!(
                "{{\"file\":\"{}\",\"function\":\"ALN_invariants\",\"parameters\":\"\",\"constraints\":[\"{}\"]}}",
                escape(&path.display().to_string()), invariants.join("\",\"")
            ));
        }
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 3 {
        eprintln!("usage: prometheus-praxis-semantics <source-root> <manifest.json>");
        std::process::exit(64);
    }
    let mut entries = Vec::new();
    scan(Path::new(&args[1]), &mut entries);
    fs::write(&args[2], format!("{{\"public_interfaces\":[{}]}}\n", entries.join(",")))
        .unwrap_or_else(|error| {
            eprintln!("cannot write manifest: {error}");
            std::process::exit(73);
        });
}
