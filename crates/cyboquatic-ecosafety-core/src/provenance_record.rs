//! Shard-ready provenance record for ecosafety envelopes.
//!
//! This mirrors the ALN particle
//! `CyboNodeEcosafetyProvenanceV1` used in
//! `CyboquaticEcosafetyProvenancePhoenix2026v1.aln`.

use crate::provenance::ProvenanceStep;
use chrono::{DateTime, Utc};

/// Single provenance row for an ecosafety envelope.
#[derive(Clone, Debug)]
pub struct EcosafetyProvenanceRecord {
    nodeid: String,
    region: String,
    window_start_utc: DateTime<Utc>,
    window_end_utc: DateTime<Utc>,
    step_index: u32,
    frame_name: &'static str,
    detail: String,
    evidencehex: String,
    signingdid: String,
}

impl EcosafetyProvenanceRecord {
    pub fn new(
        nodeid: String,
        region: String,
        window_start_utc: DateTime<Utc>,
        window_end_utc: DateTime<Utc>,
        step_index: u32,
        frame_name: &'static str,
        detail: String,
        evidencehex: String,
        signingdid: String,
    ) -> Self {
        Self {
            nodeid,
            region,
            window_start_utc,
            window_end_utc,
            step_index,
            frame_name,
            detail,
            evidencehex,
            signingdid,
        }
    }

    pub fn nodeid(&self) -> &str {
        &self.nodeid
    }
    pub fn region(&self) -> &str {
        &self.region
    }
    pub fn window_start_utc(&self) -> DateTime<Utc> {
        self.window_start_utc
    }
    pub fn window_end_utc(&self) -> DateTime<Utc> {
        self.window_end_utc
    }
    pub fn step_index(&self) -> u32 {
        self.step_index
    }
    pub fn frame_name(&self) -> &'static str {
        self.frame_name
    }
    pub fn detail(&self) -> &str {
        &self.detail
    }
    pub fn evidencehex(&self) -> &str {
        &self.evidencehex
    }
    pub fn signingdid(&self) -> &str {
        &self.signingdid
    }
}

/// Helper to compute provenance records from a sequence of steps.
pub fn steps_to_records(
    nodeid: &str,
    region: &str,
    window_start_utc: DateTime<Utc>,
    window_end_utc: DateTime<Utc>,
    evidencehex: &str,
    signingdid: &str,
    steps: &[ProvenanceStep],
) -> Vec<EcosafetyProvenanceRecord> {
    steps
        .iter()
        .enumerate()
        .map(|(idx, step)| {
            EcosafetyProvenanceRecord::new(
                nodeid.to_string(),
                region.to_string(),
                window_start_utc,
                window_end_utc,
                idx as u32,
                step.frame_name(),
                step.detail().to_string(),
                evidencehex.to_string(),
                signingdid.to_string(),
            )
        })
        .collect()
}
