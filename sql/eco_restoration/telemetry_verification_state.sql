-- File: sql/eco_restoration/telemetry_verification_state.sql

CREATE TABLE IF NOT EXISTS telemetry_verification_state (
    frame_id TEXT PRIMARY KEY,
    owner_did TEXT NOT NULL,
    canonical_frame TEXT NOT NULL,
    public_key_reference TEXT NOT NULL,
    verifier_id TEXT NOT NULL,
    verified_unix_s INTEGER NOT NULL,
    state TEXT NOT NULL CHECK(state IN ('VERIFIED','REJECTED')),
    reason TEXT NOT NULL
) STRICT;

CREATE INDEX IF NOT EXISTS telemetry_verification_did_state
ON telemetry_verification_state(owner_did, state, verified_unix_s);
