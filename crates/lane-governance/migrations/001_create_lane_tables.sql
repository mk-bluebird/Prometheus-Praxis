CREATE TABLE lane_status (
    lane_id UUID PRIMARY KEY,
    kernel_region TEXT NOT NULL,
    k_aggregate DOUBLE PRECISION NOT NULL,
    e_aggregate DOUBLE PRECISION NOT NULL,
    r_aggregate DOUBLE PRECISION NOT NULL,
    residual_trend DOUBLE PRECISION NOT NULL, -- slope b
    last_evidence_window TSTZRANGE NOT NULL,
    admissible BOOLEAN NOT NULL DEFAULT TRUE,
    reward_multiplier DOUBLE PRECISION NOT NULL DEFAULT 1.0
);

CREATE TABLE lane_governance (
    id UUID PRIMARY KEY,
    lane_id UUID NOT NULL REFERENCES lane_status(lane_id),
    predicate JSONB NOT NULL,
    reward_multiplier DOUBLE PRECISION NOT NULL,
    updated_by_did TEXT NOT NULL,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE lane_audit (
    audit_id UUID PRIMARY KEY,
    lane_id UUID NOT NULL REFERENCES lane_status(lane_id),
    shard_id UUID,
    reason TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
