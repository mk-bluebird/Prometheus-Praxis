// Filename: crates/aln_qpudatashard_parser/src/verify.rs
#![allow(dead_code)]

use kani::proof;
use crate::{QpuDataShardRow, parse_node_row_line, compute_row_evidence_hex, AlnParseError};

fn sample_row() -> QpuDataShardRow {
    let mut row = QpuDataShardRow {
        node_id: "vault-001".to_string(),
        window_start_ts: "2026-07-06T09:00:00Z".to_string(),
        window_end_ts: "2026-07-06T09:15:00Z".to_string(),
        energy_risk: 0.30,
        hydraulics_risk: 0.20,
        biology_risk: 0.25,
        carbon_risk: 0.15,
        materials_risk: 0.20,
        dataquality_risk: 0.10,
        vt: 0.11,
        ker_k: 0.93,
        ker_e: 0.90,
        ker_r: 0.12,
        evidence_hex: String::new(),
    };
    row.evidence_hex = compute_row_evidence_hex(&row);
    row
}

#[proof]
fn parse_valid_node_row_line_succeeds() {
    let row = sample_row();
    let line = format!(
        "node_row {}|{}|{}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{}",
        row.node_id,
        row.window_start_ts,
        row.window_end_ts,
        row.energy_risk,
        row.hydraulics_risk,
        row.biology_risk,
        row.carbon_risk,
        row.materials_risk,
        row.dataquality_risk,
        row.vt,
        row.ker_k,
        row.ker_e,
        row.ker_r,
        row.evidence_hex,
    );

    let parsed = parse_node_row_line(&line);
    match parsed {
        Ok(p) => {
            kani::assert_eq!(p.node_id, row.node_id);
            kani::assert_eq!(p.evidence_hex, row.evidence_hex);
        }
        Err(e) => {
            match e {
                AlnParseError::EvidenceMismatch { .. } => {
                    kani::assert!(false, "evidence should match for valid line");
                }
                _ => {
                    kani::assert!(false, "valid line should not produce parse error");
                }
            }
        }
    }
}

#[proof]
fn evidence_mismatch_is_detected() {
    let row = sample_row();
    let tampered_evidence = "0xdeadbeef00000000";

    let line = format!(
        "node_row {}|{}|{}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{}",
        row.node_id,
        row.window_start_ts,
        row.window_end_ts,
        row.energy_risk,
        row.hydraulics_risk,
        row.biology_risk,
        row.carbon_risk,
        row.materials_risk,
        row.dataquality_risk,
        row.vt,
        row.ker_k,
        row.ker_e,
        row.ker_r,
        tampered_evidence,
    );

    let parsed = parse_node_row_line(&line);
    match parsed {
        Ok(_) => {
            kani::assert!(false, "tampered evidence should not parse as valid");
        }
        Err(e) => {
            if let AlnParseError::EvidenceMismatch { expected, computed } = e {
                kani::assert_eq!(expected, tampered_evidence.to_string());
                kani::assert_ne!(expected, computed);
            } else {
                kani::assert!(false, "expected EvidenceMismatch error");
            }
        }
    }
}
