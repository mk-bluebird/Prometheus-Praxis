-- crates/zoning-shards/migrations/001_create_zoning.sql

CREATE TABLE zoning_shards (
    shard_id UUID PRIMARY KEY,
    region_id TEXT NOT NULL,
    zone_code TEXT NOT NULL,
    regulation JSONB NOT NULL,
    ker_weight DOUBLE PRECISION NOT NULL,
    effective_from DATE NOT NULL,
    source_document_hash TEXT NOT NULL
);

CREATE INDEX idx_zoning_region_zone
ON zoning_shards(region_id, zone_code);

CREATE TABLE zoning_plan_reviews (
    plan_id UUID PRIMARY KEY,
    shard_id UUID NOT NULL REFERENCES zoning_shards(shard_id),
    steward_did TEXT NOT NULL,
    reviewed_at TIMESTAMPTZ NOT NULL,
    approved BOOLEAN NOT NULL,
    legal_disclaimer BOOLEAN NOT NULL DEFAULT TRUE
);
