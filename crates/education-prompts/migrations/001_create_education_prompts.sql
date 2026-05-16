CREATE TABLE education_prompts (
    prompt_id UUID PRIMARY KEY,
    topic TEXT NOT NULL,
    difficulty SMALLINT NOT NULL CHECK (difficulty BETWEEN 1 AND 5),
    content TEXT NOT NULL,
    prerequisites TEXT[] NOT NULL DEFAULT '{}',
    ker_k DOUBLE PRECISION NOT NULL,
    ker_e DOUBLE PRECISION NOT NULL,
    ker_r DOUBLE PRECISION NOT NULL,
    active BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE education_assessments (
    prompt_id UUID PRIMARY KEY REFERENCES education_prompts(prompt_id),
    questions JSONB NOT NULL,
    passing_threshold DOUBLE PRECISION NOT NULL
);

CREATE TABLE education_reviews (
    review_id UUID PRIMARY KEY,
    prompt_id UUID NOT NULL REFERENCES education_prompts(prompt_id),
    reviewer_did TEXT NOT NULL,
    approved BOOLEAN NOT NULL,
    reviewed_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE MATERIALIZED VIEW education_prompt_status AS
SELECT
    ep.prompt_id,
    ep.topic,
    ep.difficulty,
    ep.ker_k,
    ep.ker_e,
    ep.ker_r,
    ep.active,
    COUNT(CASE WHEN er.approved THEN 1 END) AS approvals
FROM education_prompts ep
LEFT JOIN education_reviews er ON ep.prompt_id = er.prompt_id
GROUP BY ep.prompt_id;
