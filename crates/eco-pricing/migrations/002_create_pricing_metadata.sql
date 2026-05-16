CREATE TABLE eco_pricing_metadata (
    shard_id UUID PRIMARY KEY REFERENCES eco_pricing_shards(shard_id),
    effective_from DATE NOT NULL,
    effective_to DATE,
    obsolescence_warning DATE NOT NULL
);
