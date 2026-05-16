CREATE TABLE topology_audits (
    audit_id UUID PRIMARY KEY,
    target_id UUID NOT NULL,
    itopology DOUBLE PRECISION NOT NULL,
    rtopology DOUBLE PRECISION NOT NULL,
    missing_manifests TEXT[] NOT NULL,
    mislabelled_roles TEXT[] NOT NULL,
    contract_violations TEXT[] NOT NULL,
    drift_grace_seconds BIGINT NOT NULL,
    false_positive_probability DOUBLE PRECISION NOT NULL,
    timestamp TIMESTAMPTZ NOT NULL
);

CREATE INDEX idx_topology_target
ON topology_audits(target_id);
