// File: aln-cyboquatic-codegen/src/main.rs
use std::{env, fs, process};

fn pascal_case(value: &str) -> String {
    value.split('_')
        .filter(|part| !part.is_empty())
        .map(|part| {
            let mut chars = part.chars();
            match chars.next() {
                Some(first) => first.to_ascii_uppercase().to_string() + chars.as_str(),
                None => String::new(),
            }
        })
        .collect()
}

fn java_type(aln_type: &str) -> Option<&'static str> {
    match aln_type {
        "real" => Some("double"),
        "text" => Some("String"),
        "integer" => Some("long"),
        "boolean" => Some("boolean"),
        _ => None,
    }
}

fn parse_particle(source: &str) -> Option<(String, Vec<(String, String)>)> {
    let mut particle_name = None;
    let mut fields = Vec::new();

    for line in source.lines() {
        let line = line.trim();
        if line.starts_with("particle ") {
            particle_name = line
                .strip_prefix("particle ")?
                .split_whitespace()
                .next()
                .map(str::to_string);
            continue;
        }
        if particle_name.is_some() && line == "}" {
            break;
        }
        if particle_name.is_some() {
            let (name, declaration) = line.split_once(':')?;
            let field_type = declaration.trim().split_whitespace().next()?;
            fields.push((name.trim().to_string(), field_type.to_string()));
        }
    }

    particle_name.map(|name| (name, fields))
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 4 {
        eprintln!("usage: aln-cyboquatic-codegen <input.aln> <package> <output.java>");
        process::exit(64);
    }

    let source = fs::read_to_string(&args[1]).unwrap_or_else(|error| {
        eprintln!("cannot read {}: {}", args[1], error);
        process::exit(66);
    });
    let (particle, fields) = parse_particle(&source).unwrap_or_else(|| {
        eprintln!("missing or malformed ALN particle");
        process::exit(65);
    });

    let members: Vec<String> = fields
        .into_iter()
        .map(|(name, field_type)| {
            let java_type = java_type(&field_type).unwrap_or_else(|| {
                eprintln!("unsupported ALN field type: {}", field_type);
                process::exit(65);
            });
            format!("{} {}", java_type, name)
        })
        .collect();

    let class_name = pascal_case(&particle);
    let java = format!(
        "// Generated from {}; do not edit manually.\npackage {};\n\npublic record {}(\n    {}\n) {{}}\n",
        args[1],
        args[2],
        class_name,
        members.join(",\n    ")
    );

    fs::write(&args[3], java).unwrap_or_else(|error| {
        eprintln!("cannot write {}: {}", args[3], error);
        process::exit(73);
    });
}
