CREATE TABLE IF NOT EXISTS regional_eco_ledger_particles (
    particle_id UUID PRIMARY KEY,
    region_id TEXT NOT NULL,
    action_type TEXT NOT NULL,
    actor_did TEXT NOT NULL,
    timestamp TIMESTAMPTZ NOT NULL,
    ker_k DOUBLE PRECISION NOT NULL,
    ker_e DOUBLE PRECISION NOT NULL,
    ker_r DOUBLE PRECISION NOT NULL,
    residual_before JSONB NOT NULL,
    residual_after JSONB NOT NULL,
    corridor_bands JSONB NOT NULL,
    deliverables JSONB NOT NULL DEFAULT '[]',
    evidence_hash TEXT NOT NULL,
    nonce BIGINT NOT NULL,
    created_at TIMESTAMPTZ DEFAULT now()
);
CREATE INDEX IF NOT EXISTS idx_ledger_region_timestamp
ON regional_eco_ledger_particles(region_id, timestamp);
