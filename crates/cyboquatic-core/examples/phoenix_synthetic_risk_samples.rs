// filename: cyboquatic-core/examples/phoenix_synthetic_risk_samples.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

use cyboquatic_core::{make_phoenix_synthetic_sample, NodeRiskSample};
use csv::Writer;

/// Example binary:
/// 1. Generates synthetic NodeRiskSample rows for Phoenix nodes.
/// 2. Writes them to CSV for ESPD / ecosafety frame exploration.
/// 3. This file is non-actuating and produces ALN-ready or CSV artifacts.
fn main() {
    let mut samples: Vec<NodeRiskSample> = Vec::new();

    // Synthetic PFAS / pathogen / salinity / temperature risk bands
    // for a few Phoenix segments. Values are normalized [0,1].
    samples.push(make_phoenix_synthetic_sample(
        "PHX-PF-001",
        "Phoenix-Central",
        0.40,
        0.30,
        0.25,
        0.15,
    ));
    samples.push(make_phoenix_synthetic_sample(
        "PHX-PF-002",
        "Phoenix-Central",
        0.55,
        0.20,
        0.30,
        0.18,
    ));
    samples.push(make_phoenix_synthetic_sample(
        "PHX-PF-003",
        "Phoenix-West",
        0.25,
        0.35,
        0.20,
        0.12,
    ));
    samples.push(make_phoenix_synthetic_sample(
        "PHX-PF-004",
        "Phoenix-East",
        0.60,
        0.40,
        0.40,
        0.20,
    ));

    // Write CSV for downstream ESPD / ecosafety frames.
    let mut wtr = Writer::from_writer(std::io::stdout());
    wtr.write_record(&[
        "node_id",
        "region",
        "r_pfas",
        "r_pathogen",
        "r_salinity",
        "r_temperature",
        "ecosafety_risk",
    ])
    .expect("write header");

    for s in samples {
        wtr.write_record(&[
            s.node_id,
            s.region,
            format!("{:.4}", s.r_pfas),
            format!("{:.4}", s.r_pathogen),
            format!("{:.4}", s.r_salinity),
            format!("{:.4}", s.r_temperature),
            format!("{:.4}", s.ecosafety_risk),
        ])
        .expect("write row");
    }

    wtr.flush().expect("flush");
}
