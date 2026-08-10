// File: alncore/src/main.rs
use std::{env, fs, process};

fn snake_case(name: &str) -> String {
    match name {
        "K" => "ker_k".into(),
        "E" => "ker_e".into(),
        "R" => "ker_r".into(),
        _ => {
            let mut output = String::new();
            for (index, ch) in name.chars().enumerate() {
                if ch.is_ascii_uppercase() && index > 0 {
                    output.push('_');
                }
                output.push(ch.to_ascii_lowercase());
            }
            output
        }
    }
}

fn sql_expression(line: &str) -> Option<String> {
    let expression = line.trim();
    if expression.is_empty() || expression.starts_with("//") {
        return None;
    }

    let mut converted = expression.replace("frame.", "");
    for field in ["deltaVt", "fog_confidence", "eco_impact_value", "K", "E", "R"] {
        converted = converted.replace(field, &snake_case(field));
    }

    if let Some((field, range)) = converted.split_once(": real [") {
        let bounds = range.strip_suffix(']')?;
        let (minimum, maximum) = bounds.split_once(", ")?;
        if maximum == "inf)" {
            return Some(format!("{} >= {}", snake_case(field.trim()), minimum));
        }
        return Some(format!(
            "{} BETWEEN {} AND {}",
            snake_case(field.trim()),
            minimum,
            maximum.trim_end_matches(')')
        ));
    }

    Some(converted)
}

fn invariant_lines(source: &str) -> Vec<String> {
    let mut inside = false;
    let mut output = Vec::new();

    for line in source.lines() {
        let trimmed = line.trim();
        if trimmed.starts_with("invariant ") {
            inside = true;
            continue;
        }
        if inside && trimmed == "}" {
            break;
        }
        if inside {
            if let Some(expression) = sql_expression(trimmed) {
                output.push(expression);
            }
        }
    }
    output
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 4 {
        eprintln!("usage: alncore <input.aln> <output.sql> <table_name>");
        process::exit(64);
    }

    let source = fs::read_to_string(&args[1]).unwrap_or_else(|error| {
        eprintln!("cannot read {}: {}", args[1], error);
        process::exit(66);
    });
    let constraints = invariant_lines(&source);
    if constraints.is_empty() {
        eprintln!("no ALN invariant conditions found");
        process::exit(65);
    }

    let sql = format!(
        "-- Generated from {}\nALTER TABLE {} ADD COLUMN aln_corridor_valid INTEGER NOT NULL DEFAULT 0\nCHECK (\n    {}\n);\n",
        args[1],
        args[3],
        constraints.join("\n    AND ")
    );
    fs::write(&args[2], sql).unwrap_or_else(|error| {
        eprintln!("cannot write {}: {}", args[2], error);
        process::exit(73);
    });
}
