// File: round-trip-verify/src/main.rs
use std::{collections::BTreeMap, env, process::Command};

const KEYS: [&str; 5] = [
    "energyreqJ",
    "deltaVt",
    "knowledge_factor",
    "eco_impact_value",
    "accepted",
];

fn command_for(language: &str, fallback: &str) -> String {
    env::var(format!("CYBOQUATIC_{}_CMD", language)).unwrap_or_else(|_| fallback.into())
}

fn parse(output: &str) -> Result<BTreeMap<String, String>, String> {
    let mut values = BTreeMap::new();
    for line in output.lines() {
        let Some((key, value)) = line.trim().split_once('=') else { continue };
        let canonical = match key {
            "ecoImpact" => "eco_impact_value",
            "knowledgeFactor" => "knowledge_factor",
            other => other,
        };
        if KEYS.contains(&canonical) {
            values.insert(canonical.into(), value.trim().into());
        }
    }
    if KEYS.iter().all(|key| values.contains_key(*key)) {
        Ok(values)
    } else {
        Err(format!("incomplete assessment output: {output}"))
    }
}

fn run(language: &str, fallback: &str, telemetry: &str) -> Result<BTreeMap<String, String>, String> {
    let command = command_for(language, fallback);
    let result = Command::new("sh")
        .args(["-c", &format!("{command} {telemetry}")])
        .output()
        .map_err(|error| format!("{language}: {error}"))?;
    if !result.status.success() {
        return Err(format!(
            "{language} failed: {}",
            String::from_utf8_lossy(&result.stderr)
        ));
    }
    parse(&String::from_utf8_lossy(&result.stdout))
}

fn equivalent(reference: &BTreeMap<String, String>, candidate: &BTreeMap<String, String>) -> bool {
    KEYS.iter().all(|key| {
        if *key == "accepted" {
            return reference[*key] == candidate[*key];
        }
        let left = reference[*key].parse::<f64>().unwrap_or(f64::NAN);
        let right = candidate[*key].parse::<f64>().unwrap_or(f64::NAN);
        left.is_finite() && right.is_finite() && (left - right).abs() <= 1.0e-9
    })
}

fn main() {
    let telemetry = "0.035 4.2 0.78 900.0 2.1 0.82 0.000035 0.08";
    let implementations = [
        ("RUST", "./target/release/workload_assess"),
        ("CPP", "./build/cyboquatic_workload"),
        ("JAVA", "java -cp build/java org.prometheuspraxis.cyboquatic.WorkloadTelemetry"),
        ("KOTLIN", "./build/kotlin/workload_assess.kexe"),
        ("LUA", "luajit lua/cyboquatic/workload_assess.lua"),
    ];

    let mut results = Vec::new();
    for (language, fallback) in implementations {
        match run(language, fallback, telemetry) {
            Ok(result) => results.push((language, result)),
            Err(error) => {
                eprintln!("{error}");
                std::process::exit(1);
            }
        }
    }

    let reference = &results[0].1;
    for (language, result) in results.iter().skip(1) {
        if !equivalent(reference, result) {
            eprintln!("round-trip mismatch: RUST != {language}");
            eprintln!("RUST={reference:?}");
            eprintln!("{language}={result:?}");
            std::process::exit(1);
        }
    }
    println!("cross-language workload assessment: verified");
}
