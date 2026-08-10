// File: aln-cyboquatic-codegen/src/bin/generate_kotlin.rs
use std::{env, fs, process};

fn pascal_case(value: &str) -> String {
    value.split('_')
        .map(|part| {
            let mut chars = part.chars();
            chars.next()
                .map(|first| first.to_ascii_uppercase().to_string() + chars.as_str())
                .unwrap_or_default()
        })
        .collect()
}

fn kotlin_type(aln_type: &str) -> Option<&'static str> {
    match aln_type {
        "real" => Some("Double"),
        "text" => Some("String"),
        "integer" => Some("Long"),
        "boolean" => Some("Boolean"),
        _ => None,
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 4 {
        eprintln!("usage: generate_kotlin <input.aln> <package> <output.kt>");
        process::exit(64);
    }

    let source = fs::read_to_string(&args[1]).unwrap_or_else(|error| {
        eprintln!("cannot read {}: {}", args[1], error);
        process::exit(66);
    });

    let mut particle = String::new();
    let mut fields = Vec::new();
    let mut inside_particle = false;

    for line in source.lines().map(str::trim) {
        if let Some(name) = line.strip_prefix("particle ") {
            particle = name.split_whitespace().next().unwrap_or_default().to_string();
            inside_particle = true;
            continue;
        }
        if inside_particle && line == "}" {
            break;
        }
        if inside_particle {
            let Some((name, declaration)) = line.split_once(':') else { continue };
            let Some(aln_type) = declaration.trim().split_whitespace().next() else { continue };
            let Some(type_name) = kotlin_type(aln_type) else {
                eprintln!("unsupported ALN type: {}", aln_type);
                process::exit(65);
            };
            fields.push(format!("    val {}: {}", name.trim(), type_name));
        }
    }

    if particle.is_empty() || fields.is_empty() {
        eprintln!("missing ALN particle or fields");
        process::exit(65);
    }

    let output = format!(
        "// Generated from {}; do not edit manually.\npackage {}\n\ndata class {}(\n{}\n)\n",
        args[1],
        args[2],
        pascal_case(&particle),
        fields.join(",\n")
    );
    fs::write(&args[3], output).unwrap_or_else(|error| {
        eprintln!("cannot write {}: {}", args[3], error);
        process::exit(73);
    });
}
