CREATE TABLE disaster_scenarios (
    scenario_id UUID PRIMARY KEY,
    disaster_type TEXT NOT NULL CHECK (
        disaster_type IN ('heatwave','drought','flood','wildfire')
    ),
    region_id TEXT NOT NULL,
    severity DOUBLE PRECISION NOT NULL,
    description TEXT NOT NULL,
    linked_nodes UUID[] NOT NULL,
    emergency_protocol JSONB NOT NULL,
    resilience_interventions TEXT[] NOT NULL,
    probability DOUBLE PRECISION NOT NULL,
    cluster_id UUID,
    active BOOLEAN NOT NULL DEFAULT TRUE
);

CREATE INDEX idx_disaster_region_type
ON disaster_scenarios(region_id, disaster_type);

CREATE TABLE scenario_archive (
    scenario_id UUID PRIMARY KEY,
    archived_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
