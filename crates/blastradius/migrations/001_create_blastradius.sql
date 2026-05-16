-- crates/blastradius/migrations/001_create_blastradius.sql
CREATE TABLE blastradius_objects (
    object_id UUID PRIMARY KEY,
    object_type TEXT NOT NULL,
    scope TEXT NOT NULL,
    region_id TEXT NOT NULL,
    radius_km DOUBLE PRECISION NOT NULL,
    ker_min DOUBLE PRECISION NOT NULL,
    ker_max DOUBLE PRECISION NOT NULL,
    continuity_grade DOUBLE PRECISION NOT NULL,
    sovereignty_tags TEXT[] NOT NULL,
    governance_profile TEXT NOT NULL,
    hex_descriptor TEXT NOT NULL
);

CREATE INDEX idx_blastradius_region
ON blastradius_objects(region_id);
