// File: cyboquatic-core/src/bin/workload_assess.rs
use std::{env, process};

use cyboquatic_core::{assess_workload, WorkloadAssessment, WorkloadInput};

fn parse() -> WorkloadInput {
    let values: Vec<f64> = env::args()
        .skip(1)
        .map(|value| value.parse::<f64>())
        .collect::<Result<_, _>>()
        .unwrap_or_else(|_| {
            eprintln!("expected eight finite telemetry values");
            process::exit(64);
        });
    if values.len() != 8 {
        eprintln!("expected: flow lift efficiency runtime voltage renewable carbon biodiversity");
        process::exit(64);
    }
    WorkloadInput {
        flow_m3_s: values[0],
        lift_m: values[1],
        efficiency: values[2],
        runtime_s: values[3],
        voltage_drop_v: values[4],
        renewable_fraction: values[5],
        embodied_carbon_g_per_j: values[6],
        biodiversity_risk: values[7],
    }
}

fn main() {
    let input = parse();
    let mut output = WorkloadAssessment::default();
    if assess_workload(&input, &mut output) != 0 {
        eprintln!("invalid telemetry");
        process::exit(65);
    }
    println!("energyreqJ={}", output.energyreq_j);
    println!("deltaVt={}", output.delta_vt);
    println!("knowledge_factor={}", output.knowledge_factor);
    println!("eco_impact_value={}", output.eco_impact_value);
    println!("accepted={}", output.accepted);
}
