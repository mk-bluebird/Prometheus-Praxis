-- filename: db_plane_weights_history.sql
-- destination: eco_restoration_shard/db/db_plane_weights_history.sql

CREATE TABLE IF NOT EXISTS plane_weights_history (
    history_id UUID PRIMARY KEY,
    plane_name TEXT NOT NULL,
    weight DOUBLE PRECISION NOT NULL,
    nonoffsettable BOOLEAN NOT NULL,
    corridor_min DOUBLE PRECISION,
    corridor_max DOUBLE PRECISION,
    effective_from DATE NOT NULL,
    proposer_did TEXT NOT NULL,
    lane_trend_b DOUBLE PRECISION NOT NULL,
    snapshot_hash TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_plane_weights_history_plane_time
    ON plane_weights_history (plane_name, created_at);
