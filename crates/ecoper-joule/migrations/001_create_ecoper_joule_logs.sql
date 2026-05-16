CREATE TABLE ecoper_joule_logs (
    record_id UUID PRIMARY KEY,
    workload_id TEXT NOT NULL,
    node_id UUID NOT NULL,
    timestamp TIMESTAMPTZ NOT NULL,
    karmadelta DOUBLE PRECISION NOT NULL,
    energy_joules DOUBLE PRECISION NOT NULL,
    ecoper_joule DOUBLE PRECISION NOT NULL,
    energy_domain TEXT NOT NULL CHECK (energy_domain IN ('ACTUAL','MODELED')),
    qpu_catalog_entry UUID,
    tags TEXT[] NOT NULL DEFAULT '{}',
    evidence_hash TEXT NOT NULL,
    ker_k DOUBLE PRECISION NOT NULL,
    ker_e DOUBLE PRECISION NOT NULL,
    ker_r DOUBLE PRECISION NOT NULL
);

SELECT create_hypertable('ecoper_joule_logs', 'timestamp', if_not_exists => TRUE);

CREATE INDEX idx_ecoper_node_time
ON ecoper_joule_logs(node_id, timestamp);

CREATE INDEX idx_ecoper_workload_time
ON ecoper_joule_logs(workload_id, timestamp);
