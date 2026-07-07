//! Export helpers: pipeline output → provenance shard records.

use crate::pipeline3::EcosafetyPipelineOutput;
use crate::provenance_record::{steps_to_records, EcosafetyProvenanceRecord};

/// Convert a full pipeline output into shard-ready provenance records.
///
/// These records can then be serialised to CSV or inserted into the
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
