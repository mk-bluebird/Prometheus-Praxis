-- EcoNet central index schema for Cybercore / Prometheus-Praxis.

CREATE TABLE IF NOT EXISTS econet_index (
    id                      INTEGER PRIMARY KEY,
    filename                TEXT NOT NULL,
    repo                    TEXT NOT NULL,
    destination_hint        TEXT NOT NULL,
    primary_role            TEXT NOT NULL,   -- e.g., eco_reward_design, identity_shard, payment_guard
    language                TEXT NOT NULL,   -- e.g., Rust, ALN, text
    brain_identity_relevance INTEGER NOT NULL, -- 0-10, relevance to host brain identity
    eco_impact_focus        TEXT NOT NULL    -- semicolon-separated tags, e.g. "energy_reduction;waste_cleanup"
);

CREATE INDEX IF NOT EXISTS idx_econet_index_role
    ON econet_index (primary_role);

CREATE INDEX IF NOT EXISTS idx_econet_index_language
    ON econet_index (language);

CREATE INDEX IF NOT EXISTS idx_econet_index_brain_relevance
    ON econet_index (brain_identity_relevance);
