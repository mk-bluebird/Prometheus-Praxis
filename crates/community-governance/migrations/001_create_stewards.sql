CREATE TABLE stewards (
    steward_id UUID PRIMARY KEY,
    did TEXT UNIQUE NOT NULL,
    role_type TEXT NOT NULL CHECK (
        role_type IN ('block_steward','watershed_council','cooperative_admin')
    ),
    region_id TEXT NOT NULL,
    responsibilities JSONB NOT NULL DEFAULT '[]',
    governance_spine_node_id UUID,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX idx_stewards_region_role
ON stewards(region_id, role_type);

CREATE TABLE conflict_resolution (
    conflict_id UUID PRIMARY KEY,
    action_id UUID NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    status TEXT NOT NULL CHECK (
        status IN ('open','voting','resolved')
    )
);

CREATE TABLE voting_rounds (
    round_id UUID PRIMARY KEY,
    conflict_id UUID NOT NULL REFERENCES conflict_resolution(conflict_id),
    started_at TIMESTAMPTZ NOT NULL,
    ended_at TIMESTAMPTZ,
    outcome TEXT CHECK (outcome IN ('approved','rejected','abstain')),

    -- serialized delegation and weights from governance spine
    governance_lane TEXT NOT NULL,
    weights JSONB NOT NULL
);

CREATE TABLE votes (
    vote_id UUID PRIMARY KEY,
    round_id UUID NOT NULL REFERENCES voting_rounds(round_id),
    steward_id UUID NOT NULL REFERENCES stewards(steward_id),
    vote TEXT NOT NULL CHECK (vote IN ('yes','no','abstain')),
    weight DOUBLE PRECISION NOT NULL
);

CREATE INDEX idx_votes_round ON votes(round_id);
