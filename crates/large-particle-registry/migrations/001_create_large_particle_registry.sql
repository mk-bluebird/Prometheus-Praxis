CREATE TABLE large_particle_files (
    file_id UUID PRIMARY KEY,
    file_hash TEXT NOT NULL UNIQUE,
    total_size_bytes BIGINT NOT NULL,
    chunk_size_hint INT NOT NULL,
    content_type TEXT NOT NULL,
    summary_level_hint TEXT NOT NULL,   -- "full", "metadata_only", "statistical"
    ker_contribution DOUBLE PRECISION NOT NULL,
    uploaded_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE large_particle_blocks (
    block_id UUID PRIMARY KEY,
    file_id UUID NOT NULL REFERENCES large_particle_files(file_id),
    block_index INT NOT NULL,
    block_hash TEXT NOT NULL,
    size_bytes INT NOT NULL,
    aggregate_json JSONB NOT NULL,
    offset BIGINT NOT NULL
);

CREATE INDEX idx_large_blocks_file_index
ON large_particle_blocks(file_id, block_index);
