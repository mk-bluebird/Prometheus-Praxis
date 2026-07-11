// filename: crates/beecorridor-ledger/src/telemetry.rs

use crate::types::{BeeTelemetrySnapshot, CorridorId};
use rusqlite::{params, Connection, Result};

/// Result of attestation verification.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AttestationStatus {
    Ok,
    Failed,
}

/// Stub for ALN + TEE attestation verification.
/// In a full implementation, this would:
/// - parse eco.beecorridor.telemetry.v1.aln
/// - verify ALN signature against registered DIDs
/// - verify TEE quote against vendor roots and measurement hashes.
pub fn verify_attested_particle(
    _aln_bytes: &[u8],
    _trusted_roots: &[u8],
) -> AttestationStatus {
    // Placeholder: always treat as OK for now.
    AttestationStatus::Ok
}

/// Insert a verified telemetry snapshot into the committed ledger.
/// This function must only be called after verify_attested_particle has returned Ok.
pub fn insert_telemetry_snapshot(
    conn: &Connection,
    snapshot: &BeeTelemetrySnapshot,
    attestation_ok: AttestationStatus,
    aln_particle_id: &str,
    device_id: &str,
    evidence_hex: &str,
) -> Result<()> {
    let ok_flag = match attestation_ok {
        AttestationStatus::Ok => 1,
        AttestationStatus::Failed => 0,
    };

    conn.execute(
        r#"
        INSERT INTO telemetry_snapshot (
            snapshot_id,
            corridor_id,
            timestamp_utc,
            location_cell,
            raw_audio_features,
            classified_bee_count,
            emf_level,
            thermal_delta,
            chemical_index,
            attestation_ok,
            device_id,
            aln_particle_id,
            evidence_hex
        )
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13)
        "#,
        params![
            snapshot.snapshot_id,
            snapshot.corridor_id.0,
            snapshot.timestamp_utc,
            snapshot.location_cell,
            // raw_audio_features placeholder; real implementation would store actual feature vector.
            &[] as &[u8],
            snapshot.classified_bee_count,
            snapshot.emf_level,
            snapshot.thermal_delta,
            snapshot.chemical_index,
            ok_flag,
            device_id,
            aln_particle_id,
            evidence_hex
        ],
    )?;

    Ok(())
}
