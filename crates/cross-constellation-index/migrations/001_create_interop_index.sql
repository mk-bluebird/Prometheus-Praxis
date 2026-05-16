CREATE TABLE interop_index (
    entry_id UUID PRIMARY KEY,
    eco_shard_id TEXT NOT NULL,
    external_constellation TEXT NOT NULL,
    api_endpoint TEXT NOT NULL,
    method TEXT NOT NULL CHECK (method IN ('GET','POST','PUT')),
    mapping JSONB NOT NULL,
    ker_band JSONB NOT NULL,
    trust_anchor_did TEXT NOT NULL
);

CREATE TABLE sync_event_log (
    event_id UUID PRIMARY KEY,
    entry_id UUID NOT NULL REFERENCES interop_index(entry_id),
    correlation_id TEXT NOT NULL,
    direction TEXT NOT NULL CHECK (direction IN ('outbound','inbound')),
    initiated_by_did TEXT NOT NULL,
    status TEXT NOT NULL CHECK (status IN ('pending','success','error')),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    completed_at TIMESTAMPTZ
);

CREATE TABLE permission_registry (
    external_constellation TEXT PRIMARY KEY,
    required_trust_level TEXT NOT NULL,
    additional_requirements JSONB NOT NULL
);
