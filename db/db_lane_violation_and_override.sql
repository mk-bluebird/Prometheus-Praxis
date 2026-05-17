-- filename: db_lane_violation_and_override.sql
-- destination: eco_restoration_shard/db/db_lane_violation_and_override.sql

CREATE TABLE IF NOT EXISTS lane_violation (
    violation_id UUID PRIMARY KEY,
    lane_id UUID NOT NULL,
    kernel_region TEXT NOT NULL,
    violation_type TEXT NOT NULL,
    shard_id UUID,
    detected_at TIMESTAMPTZ NOT NULL,
    sql_table TEXT NOT NULL,
    sql_column TEXT NOT NULL,
    sql_row_id BIGINT,
    details TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS lane_promotion_lock (
    lane_id UUID PRIMARY KEY,
    state TEXT NOT NULL,
    ci_run_id UUID NOT NULL,
    acquired_at TIMESTAMPTZ NOT NULL,
    expires_at TIMESTAMPTZ NOT NULL
);

CREATE TABLE IF NOT EXISTS plane_weight_update_log (
    update_id UUID PRIMARY KEY,
    plane_name TEXT NOT NULL,
    new_weight DOUBLE PRECISION NOT NULL,
    nonoffsettable BOOLEAN NOT NULL,
    corridor_min DOUBLE PRECISION,
    corridor_max DOUBLE PRECISION,
    governance_proposal_hash TEXT NOT NULL,
    signed_by TEXT NOT NULL,
    applied_at TIMESTAMPTZ NOT NULL
);

CREATE TABLE IF NOT EXISTS sovereign_override (
    override_id UUID PRIMARY KEY,
    lane_id UUID NOT NULL,
    region_id TEXT NOT NULL,
    requester_did TEXT NOT NULL,
    reason TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL,
    expires_at TIMESTAMPTZ NOT NULL,
    original_blastradius_object UUID NOT NULL,
    new_radius_km DOUBLE PRECISION NOT NULL,
    ker_k DOUBLE PRECISION NOT NULL,
    ker_e DOUBLE PRECISION NOT NULL,
    ker_r DOUBLE PRECISION NOT NULL,
    treaty_reference TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS proposed_fix_queue (
    fix_id UUID PRIMARY KEY,
    target_id UUID NOT NULL,
    description TEXT NOT NULL,
    recursion_depth INTEGER NOT NULL,
    quarantine BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
