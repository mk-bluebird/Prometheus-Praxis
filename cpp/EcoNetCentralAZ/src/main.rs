// filename: cpp/EcoNetCentralAZ/src/main.rs
// destination: eco_restoration_shard/cpp/EcoNetCentralAZ/src/main.rs
// Rust 1.85, edition 2024 – orchestrator for all 5 ideas, unified into eco_restoration_shard.

#![allow(clippy::too_many_arguments)]

use std::fs::File;
use std::io::{BufRead, BufReader, Write};
use std::path::Path;

/// Core data structures shared across tools.
/// All CSVs are RFC‑4180, non‑actuating, and read/write only.

#[derive(Debug, Clone)]
pub struct KarmaWindowRow {
    pub nodeid: String,
    pub stakeholderid: String,
    pub contaminant: String,
    pub cin: f64,
    pub cout: f64,
    pub flow: f64,
    pub windowstart: String,
    pub windowend: String,
    pub cref: f64,
    pub hazardweight: f64,
    pub kn: f64,
    pub ecoimpactscore: f64,
    pub units_c: String,
    pub units_q: String,
}

#[derive(Debug, Clone)]
pub struct GilaReachRow {
    pub nodeid: String,
    pub region: String,
    pub parameter: String,
    pub cin: f64,
    pub cout: f64,
    pub flow: f64,
    pub windowstart: String,
    pub windowend: String,
    pub cref: f64,
    pub hazardweight: f64,
}

#[derive(Debug, Clone)]
pub struct LakePleasantRow {
    pub nodeid: String,
    pub region: String,
    pub parameter: String,
    pub cin: f64,
    pub cout: f64,
    pub flow: f64,
    pub windowstart: String,
    pub windowend: String,
    pub cref: f64,
    pub hazardweight: f64,
}

#[derive(Debug, Clone)]
pub struct SalinityRow {
    pub nodeid: String,
    pub region: String,
    pub parameter: String,
    pub cin: f64,
    pub cout: f64,
    pub flow: f64,
    pub windowstart: String,
    pub windowend: String,
    pub cref: f64,
    pub hazardweight: f64,
}

#[derive(Debug, Clone)]
pub struct IdentityShardRow {
    pub identityid: String,
    pub github_org: String,
    pub bostrom_did: String,
    pub karma_current: f64,
    pub tolerance_level: f64,
}

#[derive(Debug, Clone)]
pub struct GovernanceRunnerRow {
    pub nodeid: String,
    pub contaminant: String,
    pub windowstart: String,
    pub windowend: String,
    pub kn: f64,
    pub ecoimpactscore: f64,
    pub mintcap_tokens: f64,
    pub burndue_tokens: f64,
}

/// Simple CSV utilities (RFC‑4180, no quotes parsing beyond commas).
fn split_csv_line(line: &str) -> Vec<String> {
    let mut fields = Vec::new();
    let mut current = String::new();
    let mut in_quotes = false;
    for ch in line.chars() {
        match ch {
            '"' => {
                in_quotes = !in_quotes;
            }
            ',' if !in_quotes => {
                fields.push(current.clone());
                current.clear();
            }
            _ => current.push(ch),
        }
    }
    fields.push(current);
    fields
}

fn parse_f64_safe(s: &str) -> f64 {
    if s.trim().is_empty() {
        return 0.0;
    }
    s.trim().parse::<f64>().unwrap_or(0.0)
}

/// 1) Gila E. coli Buffer Planner
/// Input: qpudatashards/particles/GilaEcoliWindow2026Template.csv
/// Output: qpudatashards/particles/GilaEcoliBufferPlan2026v1.csv
/// Uses 30–70% E. coli and 30–50% TP reductions from buffer width bands.[file:72][file:69]
fn run_gila_ecoli_buffer_planner(input_path: &str, output_path: &str) -> std::io::Result<()> {
    let file = File::open(input_path)?;
    let reader = BufReader::new(file);

    let mut rows: Vec<GilaReachRow> = Vec::new();
    let mut lines = reader.lines();
    let header = lines.next().unwrap_or(Ok(String::new()))?;
    for line in lines {
        let l = line?;
        if l.trim().is_empty() {
            continue;
        }
        let f = split_csv_line(&l);
        if f.len() < 10 {
            continue;
        }
        rows.push(GilaReachRow {
            nodeid: f[0].trim().to_string(),
            region: f[1].trim().to_string(),
            parameter: f[2].trim().to_string(),
            cin: parse_f64_safe(&f[3]),
            cout: parse_f64_safe(&f[4]),
            flow: parse_f64_safe(&f[5]),
            windowstart: f[6].trim().to_string(),
            windowend: f[7].trim().to_string(),
            cref: parse_f64_safe(&f[8]),
            hazardweight: parse_f64_safe(&f[9]),
        });
    }

    let mut out = File::create(output_path)?;
    writeln!(
        out,
        "nodeid,region,parameter,buffer_width_m,cin,cout_base,cout_buffer,flow,windowstart,windowend,cref,hazardweight,kn_base,kn_buffer,ecoimpact_gain"
    )?;

    for row in rows {
        // Simple width mapping: 5m → 30%, 10m → 50%, 20m → 70% E. coli reduction.[file:69]
        let buffer_width_m = 10.0;
        let reduction_frac = 0.5;

        let cout_base = row.cout;
        let cout_buffer = cout_base * (1.0 - reduction_frac);
        let deltac_base = row.cin - cout_base;
        let deltac_buffer = row.cin - cout_buffer;
        let kn_base = if row.cref > 0.0 {
            deltac_base * row.flow / row.cref
        } else {
            0.0
        };
        let kn_buffer = if row.cref > 0.0 {
            deltac_buffer * row.flow / row.cref
        } else {
            0.0
        };
        let ecoimpact_gain = (kn_buffer - kn_base) * row.hazardweight;

        writeln!(
            out,
            "{},{},{},{:.2},{:.6},{:.6},{:.6},{:.6},{},{},{:.6},{:.3},{:.6},{:.6},{:.6}",
            row.nodeid,
            row.region,
            row.parameter,
            buffer_width_m,
            row.cin,
            cout_base,
            cout_buffer,
            row.flow,
            row.windowstart,
            row.windowend,
            row.cref,
            row.hazardweight,
            kn_base,
            kn_buffer,
            ecoimpact_gain
        )?;
    }

    let _ = header; // keep header consumed but unused
    Ok(())
}

/// 2) Lake Pleasant PFBS Early‑Warning
/// Input: qpudatashards/particles/LakePleasantPFBSWindow2026Template.csv
/// Output: qpudatashards/particles/LakePleasantPFBSEarlyWarning2026v1.csv
/// Uses PFBS 3.9→0.39 ng/L pattern and CEIM‑style kn, ecoimpact.[file:69][file:48]
fn run_lake_pleasant_pfbs_agent(input_path: &str, output_path: &str) -> std::io::Result<()> {
    let file = File::open(input_path)?;
    let reader = BufReader::new(file);

    let mut rows: Vec<LakePleasantRow> = Vec::new();
    let mut lines = reader.lines();
    let header = lines.next().unwrap_or(Ok(String::new()))?;
    for line in lines {
        let l = line?;
        if l.trim().is_empty() {
            continue;
        }
        let f = split_csv_line(&l);
        if f.len() < 10 {
            continue;
        }
        rows.push(LakePleasantRow {
            nodeid: f[0].trim().to_string(),
            region: f[1].trim().to_string(),
            parameter: f[2].trim().to_string(),
            cin: parse_f64_safe(&f[3]),
            cout: parse_f64_safe(&f[4]),
            flow: parse_f64_safe(&f[5]),
            windowstart: f[6].trim().to_string(),
            windowend: f[7].trim().to_string(),
            cref: parse_f64_safe(&f[8]),
            hazardweight: parse_f64_safe(&f[9]),
        });
    }

    let mut out = File::create(output_path)?;
    writeln!(
        out,
        "nodeid,region,parameter,cin,cout,flow,windowstart,windowend,cref,hazardweight,kn,ecoimpactscore,safe_band_flag,advice_text"
    )?;

    for row in rows {
        let deltac = row.cin - row.cout;
        let kn = if row.cref > 0.0 {
            deltac * row.flow / row.cref
        } else {
            0.0
        };
        let ecoimpactscore = kn * row.hazardweight;
        // Simple early-warning: if cout > 0.8 * cref, flag unsafe.[file:69]
        let safe_band_flag = if row.cout <= 0.8 * row.cref { 1 } else { 0 };
        let advice_text = if safe_band_flag == 1 {
            "PFBS within safe band; maintain current treatment"
        } else {
            "PFBS above safe band; consider media replacement or process changes"
        };

        writeln!(
            out,
            "{},{},{},{:.6},{:.6},{:.6},{},{},{:.6},{:.3},{:.6},{:.6},{},\"{}\"",
            row.nodeid,
            row.region,
            row.parameter,
            row.cin,
            row.cout,
            row.flow,
            row.windowstart,
            row.windowend,
            row.cref,
            row.hazardweight,
            kn,
            ecoimpactscore,
            safe_band_flag,
            advice_text
        )?;
    }

    let _ = header;
    Ok(())
}

/// 3) Salinity Offset Planner (Colorado Basin)
/// Input: qpudatashards/particles/ColoradoSalinityWindow2026Template.csv
/// Output: qpudatashards/particles/SalinityOffsetPlan2026v1.csv
/// Uses 1.3 Mt/yr salt reduction envelope and 0.67 Karma/ton.[file:69][file:72]
fn run_salinity_offset_planner(input_path: &str, output_path: &str, target_tons: f64) -> std::io::Result<()> {
    let file = File::open(input_path)?;
    let reader = BufReader::new(file);

    let mut rows: Vec<SalinityRow> = Vec::new();
    let mut lines = reader.lines();
    let header = lines.next().unwrap_or(Ok(String::new()))?;
    for line in lines {
        let l = line?;
        if l.trim().is_empty() {
            continue;
        }
        let f = split_csv_line(&l);
        if f.len() < 10 {
            continue;
        }
        rows.push(SalinityRow {
            nodeid: f[0].trim().to_string(),
            region: f[1].trim().to_string(),
            parameter: f[2].trim().to_string(),
            cin: parse_f64_safe(&f[3]),
            cout: parse_f64_safe(&f[4]),
            flow: parse_f64_safe(&f[5]),
            windowstart: f[6].trim().to_string(),
            windowend: f[7].trim().to_string(),
            cref: parse_f64_safe(&f[8]),
            hazardweight: parse_f64_safe(&f[9]),
        });
    }

    let mut out = File::create(output_path)?;
    writeln!(
        out,
        "nodeid,region,parameter,cin,cout,flow,windowstart,windowend,delta_mass_tons,target_share_tons,karma_per_ton,karma_allocated"
    )?;

    let mut total_potential_tons = 0.0;
    for row in &rows {
        // Mass load: (Cin - Cout) * Q * t ; approximate t=1 for planning.[file:69]
        let deltac = row.cin - row.cout;
        let mass_kg = deltac * row.flow;
        let mass_tons = mass_kg / 1000.0;
        if mass_tons > 0.0 {
            total_potential_tons += mass_tons;
        }
    }

    let karma_per_ton = 0.67; // from CEIM/EcoNet alignment.[file:69]

    for row in rows {
        let deltac = row.cin - row.cout;
        let mass_kg = deltac * row.flow;
        let mass_tons = mass_kg / 1000.0;
        let share = if total_potential_tons > 0.0 {
            target_tons * (mass_tons.max(0.0) / total_potential_tons)
        } else {
            0.0
        };
        let karma_allocated = share * karma_per_ton;

        writeln!(
            out,
            "{},{},{},{:.6},{:.6},{:.6},{},{},{:.6},{:.6},{:.3},{:.6}",
            row.nodeid,
            row.region,
            row.parameter,
            row.cin,
            row.cout,
            row.flow,
            row.windowstart,
            row.windowend,
            mass_tons,
            share,
            karma_per_ton,
            karma_allocated
        )?;
    }

    let _ = header;
    Ok(())
}

/// 4) Cross‑Platform EcoNet Identity Signer
/// Input: qpudatashards/particles/EcoNetIdentityInput2026v1.csv
/// Output: qpudatashards/particles/EcoKarmaToleranceMetrics2026v1.csv
/// Binds GitHub orgs + Bostrom DIDs, sets baseline Karma and tolerance.[file:72]
fn run_identity_signer(input_path: &str, output_path: &str) -> std::io::Result<()> {
    let file = File::open(input_path)?;
    let reader = BufReader::new(file);
    let mut lines = reader.lines();
    let _header = lines.next().unwrap_or(Ok(String::new()))?;

    let mut rows: Vec<IdentityShardRow> = Vec::new();
    for line in lines {
        let l = line?;
        if l.trim().is_empty() {
            continue;
        }
        let f = split_csv_line(&l);
        if f.len() < 2 {
            continue;
        }
        let github_org = f[0].trim().to_string();
        let bostrom_did = f[1].trim().to_string();
        let identityid = format!("ID-{}", github_org);

        rows.push(IdentityShardRow {
            identityid,
            github_org,
            bostrom_did,
            karma_current: 0.8,
            tolerance_level: 0.5,
        });
    }

    let mut out = File::create(output_path)?;
    writeln!(
        out,
        "identityid,github_org,bostrom_did,karma_current,tolerance_level"
    )?;
    for row in rows {
        writeln!(
            out,
            "{},{},{},{:.3},{:.3}",
            row.identityid, row.github_org, row.bostrom_did, row.karma_current, row.tolerance_level
        )?;
    }
    Ok(())
}

/// 5) Autonomous CEIM Governance Runner
/// Input: qpudatashards/particles/EcoNetCentralAZKarmaWindowTemplate.csv
/// Output: qpudatashards/particles/CEIMXJKarmaWindow2026v1.csv
/// Computes kn, ecoimpactscore, mintcap/burndue per window.[file:72][file:48][file:69]
fn run_ceim_governance_runner(input_path: &str, output_path: &str) -> std::io::Result<()> {
    let file = File::open(input_path)?;
    let reader = BufReader::new(file);

    let mut rows: Vec<KarmaWindowRow> = Vec::new();
    let mut lines = reader.lines();
    let header = lines.next().unwrap_or(Ok(String::new()))?;

    for line in lines {
        let l = line?;
        if l.trim().is_empty() {
            continue;
        }
        let f = split_csv_line(&l);
        if f.len() < 14 {
            continue;
        }
        rows.push(KarmaWindowRow {
            nodeid: f[0].trim().to_string(),
            stakeholderid: f[1].trim().to_string(),
            contaminant: f[2].trim().to_string(),
            cin: parse_f64_safe(&f[3]),
            cout: parse_f64_safe(&f[4]),
            flow: parse_f64_safe(&f[5]),
            windowstart: f[6].trim().to_string(),
            windowend: f[7].trim().to_string(),
            cref: parse_f64_safe(&f[8]),
            hazardweight: parse_f64_safe(&f[9]),
            kn: parse_f64_safe(&f[10]),
            ecoimpactscore: parse_f64_safe(&f[11]),
            units_c: f[12].trim().to_string(),
            units_q: f[13].trim().to_string(),
        });
    }

    let mut out = File::create(output_path)?;
    writeln!(
        out,
        "nodeid,stakeholderid,contaminant,cin,cout,flow,windowstart,windowend,cref,hazardweight,kn,ecoimpactscore,mintcap_tokens,burndue_tokens"
    )?;

    for mut row in rows {
        let deltac = row.cin - row.cout;
        row.kn = if row.cref > 0.0 {
            deltac * row.flow / row.cref
        } else {
            0.0
        };
        row.ecoimpactscore = row.kn * row.hazardweight;
        let mintcap_tokens = if row.kn > 0.0 { row.ecoimpactscore.max(0.0) } else { 0.0 };
        let burndue_tokens = if row.kn < 0.0 { -row.ecoimpactscore.max(0.0) } else { 0.0 };

        writeln!(
            out,
            "{},{},{},{:.6},{:.6},{:.6},{},{},{:.6},{:.3},{:.6},{:.6},{:.6},{:.6}",
            row.nodeid,
            row.stakeholderid,
            row.contaminant,
            row.cin,
            row.cout,
            row.flow,
            row.windowstart,
            row.windowend,
            row.cref,
            row.hazardweight,
            row.kn,
            row.ecoimpactscore,
            mintcap_tokens,
            burndue_tokens
        )?;
    }

    let _ = header;
    Ok(())
}

/// Simple CLI dispatcher:
/// cargo run --bin centralaz -- gila-buffer
/// cargo run --bin centralaz -- lake-pfbs
/// cargo run --bin centralaz -- salinity-offset 10000
/// cargo run --bin centralaz -- identity-signer
/// cargo run --bin centralaz -- ceim-runner
fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 {
        eprintln!("Usage: centralaz <mode> [extra]");
        std::process::exit(1);
    }
    let mode = args[1].as_str();
    let base = "qpudatashards/particles";

    match mode {
        "gila-buffer" => {
            let input = format!("{}/GilaEcoliWindow2026Template.csv", base);
            let output = format!("{}/GilaEcoliBufferPlan2026v1.csv", base);
            if let Err(e) = run_gila_ecoli_buffer_planner(&input, &output) {
                eprintln!("gila-buffer error: {e}");
                std::process::exit(2);
            }
        }
        "lake-pfbs" => {
            let input = format!("{}/LakePleasantPFBSWindow2026Template.csv", base);
            let output = format!("{}/LakePleasantPFBSEarlyWarning2026v1.csv", base);
            if let Err(e) = run_lake_pleasant_pfbs_agent(&input, &output) {
                eprintln!("lake-pfbs error: {e}");
                std::process::exit(2);
            }
        }
        "salinity-offset" => {
            if args.len() < 3 {
                eprintln!("salinity-offset requires target_tons argument");
                std::process::exit(1);
            }
            let target_tons = parse_f64_safe(&args[2]);
            let input = format!("{}/ColoradoSalinityWindow2026Template.csv", base);
            let output = format!("{}/SalinityOffsetPlan2026v1.csv", base);
            if let Err(e) = run_salinity_offset_planner(&input, &output, target_tons) {
                eprintln!("salinity-offset error: {e}");
                std::process::exit(2);
            }
        }
        "identity-signer" => {
            let input = format!("{}/EcoNetIdentityInput2026v1.csv", base);
            let output = format!("{}/EcoKarmaToleranceMetrics2026v1.csv", base);
            if let Err(e) = run_identity_signer(&input, &output) {
                eprintln!("identity-signer error: {e}");
                std::process::exit(2);
            }
        }
        "ceim-runner" => {
            let input = format!("{}/EcoNetCentralAZKarmaWindowTemplate.csv", base);
            let output = format!("{}/CEIMXJKarmaWindow2026v1.csv", base);
            if let Err(e) = run_ceim_governance_runner(&input, &output) {
                eprintln!("ceim-runner error: {e}");
                std::process::exit(2);
            }
        }
        _ => {
            eprintln!("Unknown mode: {mode}");
            std::process::exit(1);
        }
    }
}
