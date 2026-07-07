//! Export helpers: pipeline output → provenance shard records.
//!
//! These functions remain non‑actuating. They convert in‑memory
//! diagnostics into shard‑ready provenance records and CSV rows.

use crate::pipeline3::EcosafetyPipelineOutput;
use crate::provenance_record::{steps_to_records, EcosafetyProvenanceRecord};

/// Convert a full pipeline output into shard‑ready provenance records.
///
/// The returned records can be serialised to CSV or inserted into the
/// `CyboNodeEcosafetyProvenanceV1` shard by an external writer.
pub fn pipeline_output_to_provenance_records(
    output: &EcosafetyPipelineOutput,
) -> Vec<EcosafetyProvenanceRecord> {
    let env = output.envelope();
    let prov = output.provenance();

    steps_to_records(
        env.nodeid(),
        env.region(),
        env.window_start_utc(),
        env.window_end_utc(),
        env.evidencehex(),
        env.signingdid(),
        prov.steps(),
    )
}

/// Serialise a provenance record to a single CSV row string.
///
/// `delimiter` is typically `b','`. Timestamps are encoded in RFC 3339.
pub fn provenance_record_to_csv_row(
    rec: &EcosafetyProvenanceRecord,
    delimiter: u8,
) -> String {
    let sep = delimiter as char;
    let sep_s = sep.to_string();

    let cols = [
        rec.nodeid().to_string(),
        rec.region().to_string(),
        rec.window_start_utc().to_rfc3339(),
        rec.window_end_utc().to_rfc3339(),
        rec.step_index().to_string(),
        rec.frame_name().to_string(),
        rec.detail().to_string(),
        rec.evidencehex().to_string(),
        rec.signingdid().to_string(),
    ];

    cols.join(&sep_s)
}
