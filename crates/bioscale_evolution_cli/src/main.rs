// File: crates/bioscale_evolution_cli/src/main.rs
#![forbid(unsafe_code)]

mod tx_builder;

use rusqlite::{params, Connection, OptionalExtension};
use std::{env, process};

const DEFAULT_ECO_TARGET: f64 = 0.85;
const DEFAULT_RESIDUAL_TARGET: f64 = 0.20;

#[derive(Debug)]
struct Arguments {
    database_path: String,
    canal_node: Option<String>,
    eco_target: f64,
    residual_target: f64,
    eco_multiplier_base: f64,
    r_axis: f64,
}

#[derive(Debug)]
struct Recommendation {
    canal_node: String,
    sample_count: i64,
    historical_eco_impact: f64,
    historical_delta_vt: f64,
    historical_efficiency: f64,
    historical_renewable_fraction: f64,
    efficiency_target: f64,
    renewable_fraction_target: f64,
}

fn parse_unit(name: &str, value: &str) -> Result<f64, String> {
    let parsed = value
        .parse::<f64>()
        .map_err(|_| format!("{name} must be a finite number"))?;
    if !parsed.is_finite() || !(0.0..=1.0).contains(&parsed) {
        return Err(format!("{name} must be within [0, 1]"));
    }
    Ok(parsed)
}

fn usage() -> &'static str {
    "usage: bioscale-evolution --db <telemetry.sqlite> [--canal-node <id>] \
[--eco-target <0..1>] [--residual-target <0..1>] \
[--eco-multiplier-base <0..1>] [--r-axis <0..1>]"
}

fn arguments() -> Result<Arguments, String> {
    let mut database_path = None;
    let mut canal_node = None;
    let mut eco_target = DEFAULT_ECO_TARGET;
    let mut residual_target = DEFAULT_RESIDUAL_TARGET;
    let mut eco_multiplier_base = 0.0;
    let mut r_axis = 0.0;
    let mut values = env::args().skip(1);

    while let Some(flag) = values.next() {
        let value = values
            .next()
            .ok_or_else(|| format!("missing value for {flag}"))?;
        match flag.as_str() {
            "--db" => database_path = Some(value),
            "--canal-node" => {
                if value.trim().is_empty() {
                    return Err("canal node must not be empty".into());
                }
                canal_node = Some(value);
            }
            "--eco-target" => eco_target = parse_unit("eco target", &value)?,
            "--residual-target" => residual_target = parse_unit("residual target", &value)?,
            "--eco-multiplier-base" => {
                eco_multiplier_base = parse_unit("eco multiplier base", &value)?
            }
            "--r-axis" => r_axis = parse_unit("r axis", &value)?,
            _ => return Err(format!("unknown option: {flag}")),
        }
    }

    Ok(Arguments {
        database_path: database_path.ok_or_else(|| "missing --db".to_string())?,
        canal_node,
        eco_target,
        residual_target,
        eco_multiplier_base,
        r_axis,
    })
}

fn optimize(
    connection: &Connection,
    arguments: &Arguments,
) -> rusqlite::Result<Option<Recommendation>> {
    let sql = "
        SELECT
            canal_node,
            COUNT(*),
            AVG(eco_impact_value),
            AVG(delta_vt),
            AVG(efficiency),
            AVG(renewable_fraction)
        FROM cyboquatic_workload_frame
        WHERE efficiency IS NOT NULL
          AND renewable_fraction IS NOT NULL
          AND (?1 IS NULL OR canal_node = ?1)
        GROUP BY canal_node
        ORDER BY AVG(eco_impact_value) ASC, AVG(delta_vt) DESC
        LIMIT 1";

    connection
        .query_row(sql, params![arguments.canal_node], |row| {
            let canal_node: String = row.get(0)?;
            let sample_count: i64 = row.get(1)?;
            let eco_impact: f64 = row.get(2)?;
            let delta_vt: f64 = row.get(3)?;
            let efficiency: f64 = row.get(4)?;
            let renewable_fraction: f64 = row.get(5)?;

            let eco_gap = (arguments.eco_target - eco_impact).max(0.0);
            let residual_gap = (delta_vt - arguments.residual_target).max(0.0);
            let efficiency_target =
                (efficiency + 0.50 * eco_gap + 0.20 * residual_gap).clamp(0.0, 1.0);
            let renewable_fraction_target =
                (renewable_fraction + 0.65 * eco_gap + 0.25 * residual_gap).clamp(0.0, 1.0);

            Ok(Recommendation {
                canal_node,
                sample_count,
                historical_eco_impact: eco_impact,
                historical_delta_vt: delta_vt,
                historical_efficiency: efficiency,
                historical_renewable_fraction: renewable_fraction,
                efficiency_target,
                renewable_fraction_target,
            })
        })
        .optional()
}

fn main() {
    let arguments = arguments().unwrap_or_else(|error| {
        eprintln!("{error}\n{}", usage());
        process::exit(64);
    });

    let connection = Connection::open(&arguments.database_path).unwrap_or_else(|error| {
        eprintln!("cannot open {}: {error}", arguments.database_path);
        process::exit(66);
    });

    let recommendation = optimize(&connection, &arguments).unwrap_or_else(|error| {
        eprintln!("telemetry query failed: {error}");
        process::exit(65);
    });

    let Some(result) = recommendation else {
        eprintln!("no matching telemetry rows contain efficiency and renewable_fraction");
        process::exit(65);
    };

    let gas_equivalent = (result.sample_count.max(1) as u64)
        .saturating_mul(1_000);
    let contribution = tx_builder::build_eco_contribution_plan(
        gas_equivalent,
        arguments.eco_multiplier_base,
        arguments.r_axis,
    )
    .unwrap_or_else(|_| {
        eprintln!("invalid eco-contribution planning inputs");
        process::exit(65);
    });

    println!("canal_node={}", result.canal_node);
    println!("sample_count={}", result.sample_count);
    println!("historical_eco_impact={:.6}", result.historical_eco_impact);
    println!("historical_delta_vt={:.6}", result.historical_delta_vt);
    println!("historical_efficiency={:.6}", result.historical_efficiency);
    println!(
        "historical_renewable_fraction={:.6}",
        result.historical_renewable_fraction
    );
    println!("efficiency_target={:.6}", result.efficiency_target);
    println!(
        "renewable_fraction_target={:.6}",
        result.renewable_fraction_target
    );
    if let Some(plan) = contribution {
        println!("estimated_eco_contribution_boot={}", plan.estimated_boot);
        println!("eco_multiplier={:.6}", plan.eco_multiplier);
        println!("r_axis_ratio={:.6}", plan.risk_ratio);
    }
}
