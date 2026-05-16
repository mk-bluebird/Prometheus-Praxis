CREATE TABLE steward_knowledge_state (
    steward_did TEXT PRIMARY KEY,
    completed_prompts TEXT[] NOT NULL DEFAULT '{}',
    knowledge_multiplier DOUBLE PRECISION NOT NULL DEFAULT 1.0,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
