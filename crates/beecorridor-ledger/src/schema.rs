// filename: crates/beecorridor-ledger/src/schema.rs

use rusqlite::{Connection, Result};

/// Run migrations to create the BeeCorridorLedger schema.
/// This function is idempotent and safe to call at startup.
pub fn run_migrations(conn: &Connection) -> Result<()> {
    conn.execute_batch(
        r#"
        PRAGMA foreign_keys = ON;

        -- Corridor definitions: safe/gold/hard bands per risk plane.
        CREATE TABLE IF NOT EXISTS corridordefinition (
            corridor_id      TEXT NOT NULL,
            plane_name       TEXT NOT NULL,
            safe_lo          REAL NOT NULL,
            safe_hi          REAL NOT NULL,
            gold_lo          REAL NOT NULL,
            gold_hi          REAL NOT NULL,
            hard_lo          REAL NOT NULL,
            hard_hi          REAL NOT NULL,
            evidence_id      TEXT NOT NULL,
            signing_did      TEXT NOT NULL,
            evidence_hex     TEXT NOT NULL,
            version_tag      TEXT NOT NULL,
            PRIMARY KEY (corridor_id, plane_name, version_tag)
        );

        -- Plane weights for Lyapunov residual; bee planes must have >= 70% mass.
        CREATE TABLE IF NOT EXISTS planeweights (
            shard_id         TEXT NOT NULL,
            plane_name       TEXT NOT NULL,
            weight           REAL NOT NULL,
            is_bee_plane     INTEGER NOT NULL CHECK (is_bee_plane IN (0,1)),
            non_offsettable  INTEGER NOT NULL CHECK (non_offsettable IN (0,1)),
            signing_did      TEXT NOT NULL,
            evidence_hex     TEXT NOT NULL,
            version_tag      TEXT NOT NULL,
            PRIMARY KEY (shard_id, plane_name, version_tag)
        );

        CREATE VIEW IF NOT EXISTS v_bee_weight_mass AS
        SELECT shard_id,
               SUM(CASE WHEN is_bee_plane = 1 THEN weight ELSE 0.0 END) AS bee_mass,
               SUM(weight) AS total_mass
        FROM planeweights
        GROUP BY shard_id;

        -- Telemetry snapshots admitted after ALN + TEE attestation checks.
        CREATE TABLE IF NOT EXISTS telemetry_snapshot (
            snapshot_id          TEXT PRIMARY KEY,
            corridor_id          TEXT NOT NULL,
            timestamp_utc        INTEGER NOT NULL,
            location_cell        TEXT NOT NULL,
            raw_audio_features   BLOB NOT NULL,
            classified_bee_count INTEGER NOT NULL,
            emf_level            REAL NOT NULL,
            thermal_delta        REAL NOT NULL,
            chemical_index       REAL NOT NULL,
            attestation_ok       INTEGER NOT NULL CHECK (attestation_ok IN (0,1)),
            device_id            TEXT NOT NULL,
            aln_particle_id      TEXT NOT NULL,
            evidence_hex         TEXT NOT NULL
        );

        -- BeeCorridor credits for economic incentives.
        CREATE TABLE IF NOT EXISTS beecorridor_credit (
            cell_id        TEXT PRIMARY KEY,
            balance_brc    REAL NOT NULL,
            last_update_utc INTEGER NOT NULL,
            evidence_hex   TEXT NOT NULL
        );

        -- Records of Kani proofs for governance-critical functions.
        CREATE TABLE IF NOT EXISTS drkerkani (
            record_id      TEXT PRIMARY KEY,
            func_name      TEXT NOT NULL,
            harness_name   TEXT NOT NULL,
            kani_version   TEXT NOT NULL,
            rust_commit    TEXT NOT NULL,
            proof_status   TEXT NOT NULL,
            proof_timestamp_utc INTEGER NOT NULL
        );
        "#,
    )
}
