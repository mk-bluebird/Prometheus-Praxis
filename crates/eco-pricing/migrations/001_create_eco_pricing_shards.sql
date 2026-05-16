CREATE TABLE eco_pricing_shards (
    shard_id UUID PRIMARY KEY,
    intervention_id TEXT NOT NULL,
    cost_currency TEXT NOT NULL,
    capex_per_unit DOUBLE PRECISION NOT NULL,
    opex_per_unit DOUBLE PRECISION NOT NULL,
    benefits JSONB NOT NULL,
    ker_k DOUBLE PRECISION NOT NULL,
    ker_e DOUBLE PRECISION NOT NULL,
    ker_r DOUBLE PRECISION NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX idx_eco_pricing_intervention
ON eco_pricing_shards(intervention_id);
