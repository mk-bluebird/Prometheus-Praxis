// File: crates/fog_router/src/bin/fog_router_stdio.rs
use std::io::{self, BufRead, Write};

use fog_router::{
    validate_and_route, BlastRadiusKernel, BlastRadiusResult, GateDecision, WorkloadFrame,
};

struct CorridorKernel;

impl BlastRadiusKernel for CorridorKernel {
    fn evaluate(
        &self,
        energyreq_j: f64,
        predicted_surcharge_radius_m: f64,
        permitted_surcharge_radius_m: f64,
    ) -> BlastRadiusResult {
        BlastRadiusResult {
            predicted_radius_m: predicted_surcharge_radius_m,
            residual_delta: if energyreq_j.is_finite() && energyreq_j >= 0.0 {
                0.0
            } else {
                1.0
            },
            within_corridor: predicted_surcharge_radius_m <= permitted_surcharge_radius_m,
        }
    }
}

fn parse_frame(line: &str) -> Result<WorkloadFrame, &'static str> {
    let fields: Vec<&str> = line.trim_end().split('\t').collect();
    if fields.len() != 8 || fields[0].trim().is_empty() {
        return Err("expected canal_node and seven numeric fields");
    }

    let parse = |index: usize| {
        fields[index]
            .parse::<f64>()
            .map_err(|_| "invalid numeric workload field")
    };

    Ok(WorkloadFrame {
        canal_node: fields[0].to_owned(),
        energyreq_j: parse(1)?,
        delta_vt: parse(2)?,
        ker_k: parse(3)?,
        ker_e: parse(4)?,
        ker_r: parse(5)?,
        fog_confidence: parse(6)?,
        predicted_surcharge_radius_m: parse(7)?,
        permitted_surcharge_radius_m: 250.0,
    })
}

fn main() {
    let stdin = io::stdin();
    let mut stdout = io::stdout().lock();
    let kernel = CorridorKernel;

    for line in stdin.lock().lines() {
        let response = match line {
            Ok(line) => match parse_frame(&line) {
                Ok(frame) => match validate_and_route(frame, &kernel, 0.35, 0.75) {
                    GateDecision::Admit => "ACCEPT\tcorridor-verified\n",
                    GateDecision::Review => "REVIEW\tcorridor-or-governance-review-required\n",
                },
                Err(reason) => {
                    let _ = writeln!(stdout, "REVIEW\t{reason}");
                    let _ = stdout.flush();
                    continue;
                }
            },
            Err(_) => "REVIEW\tinput-read-failure\n",
        };
        let _ = stdout.write_all(response.as_bytes());
        let _ = stdout.flush();
    }
}
