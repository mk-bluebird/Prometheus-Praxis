-- filepath: sql/health_data_tcr/2026_06_add_aichat_hashonly.sql
-- Purpose:
--   Add hash-only AI chat summary fields to the health_data_tcr schema,
--   consistent with AiChatSummaryV1 and the ecosafety / neurorights spine.
--   No raw chat text is ever stored; only hashes and coarse, non-identifying
--   metadata are persisted.

BEGIN;

-- 1. Table: health_data_tcr_ai_chat_summary_v1
--    New table for hash-only AI chat summaries, keyed by a pseudonymous
--    health_data_tcr subject identifier and a KO/episode id.

CREATE TABLE IF NOT EXISTS health_data_tcr_ai_chat_summary_v1 (
    -- Pseudonymous subject or record identifier, matching existing TCR IDs.
    tcr_id                  TEXT        NOT NULL,

    -- Knowledge-object episode identifier for this chat summary.
    ko_id                   TEXT        NOT NULL,

    -- Hash of a normalized topic vector for the conversation.
    -- This is a fixed-length hex string from a non-blacklisted hash function.
    topic_vector_hash_hex   TEXT        NOT NULL,

    -- Hash of the normalized chat transcript (HASHONLY, no raw text).
    transcript_hash_hex     TEXT        NOT NULL,

    -- Optional hash of a stable participant set / role configuration.
    participant_set_hash_hex TEXT       NULL,

    -- High-level, bounded categorical label for the interaction
    -- (e.g., "CHECK_IN", "SYMTOM_REVIEW", "COACHING").
    -- This must be coarse and non-identifying by design.
    interaction_label       TEXT        NOT NULL,

    -- UTC timestamps for traceability.
    created_at_utc          TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at_utc          TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    -- Primary key at (tcr_id, ko_id) to align with KO patterns.
    CONSTRAINT pk_tcr_ai_chat_summary_v1
        PRIMARY KEY (tcr_id, ko_id),

    -------------------------------------------------------------------
    -- CHECK CONSTRAINTS
    -------------------------------------------------------------------

    -- Enforce hex-only, fixed-length hashes (e.g., 64 hex chars).
    CONSTRAINT chk_ai_chat_topic_vector_hash_hex_format
        CHECK (topic_vector_hash_hex ~ '^[0-9a-f]{64}$'),

    CONSTRAINT chk_ai_chat_transcript_hash_hex_format
        CHECK (transcript_hash_hex ~ '^[0-9a-f]{64}$'),

    CONSTRAINT chk_ai_chat_participant_set_hash_hex_format
        CHECK (
            participant_set_hash_hex IS NULL
            OR participant_set_hash_hex ~ '^[0-9a-f]{64}$'
        ),

    -- Enforce non-empty, coarse-grained interaction label bounded length
    -- to discourage leaking fine-grained or identifying detail.
    CONSTRAINT chk_ai_chat_interaction_label_length
        CHECK (
            interaction_label IS NOT NULL
            AND length(interaction_label) BETWEEN 3 AND 64
        ),

    -- Hard guard: this table must never contain raw chat text fields.
    -- This is expressed as a structural invariant: any attempt to add
    -- text_body-like columns must violate CI, not runtime state.
    -- (CI preflight will scan for forbidden column names.)
    --
    -- Here we add a runtime assertion that hashes are present whenever
    -- a record exists, so summaries are always HASHONLY.
    CONSTRAINT chk_ai_chat_hashonly_required
        CHECK (
            topic_vector_hash_hex IS NOT NULL
            AND transcript_hash_hex IS NOT NULL
        )
);

COMMENT ON TABLE health_data_tcr_ai_chat_summary_v1 IS
    'Hash-only AI chat summaries (AiChatSummaryV1). Stores only hashes and coarse labels, never raw text.';

COMMENT ON COLUMN health_data_tcr_ai_chat_summary_v1.topic_vector_hash_hex IS
    'Hex-encoded hash of normalized topic vector; derived from model embeddings but stored only as a hash.';

COMMENT ON COLUMN health_data_tcr_ai_chat_summary_v1.transcript_hash_hex IS
    'Hex-encoded hash of the normalized transcript; no raw text stored.';

COMMENT ON COLUMN health_data_tcr_ai_chat_summary_v1.participant_set_hash_hex IS
    'Optional hex-encoded hash of the participant/role set; used for consistency checks without exposing identities.';

COMMENT ON COLUMN health_data_tcr_ai_chat_summary_v1.interaction_label IS
    'Coarse, non-identifying categorical descriptor of the interaction (e.g., CHECK_IN, COACHING).';


-- 2. Trigger to keep updated_at_utc in sync.

CREATE OR REPLACE FUNCTION health_data_tcr_ai_chat_summary_v1_touch_updated_at()
RETURNS trigger AS $$
BEGIN
    NEW.updated_at_utc := NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_health_data_tcr_ai_chat_summary_v1_touch
    ON health_data_tcr_ai_chat_summary_v1;

CREATE TRIGGER trg_health_data_tcr_ai_chat_summary_v1_touch
    BEFORE UPDATE ON health_data_tcr_ai_chat_summary_v1
    FOR EACH ROW
    EXECUTE FUNCTION health_data_tcr_ai_chat_summary_v1_touch_updated_at();


-- 3. Optional: reference from a central KO / health_data_tcr event table.
--    This assumes an existing health_data_tcr_event table; adapt names
--    if your KO index table differs.

ALTER TABLE IF EXISTS health_data_tcr_event
    ADD COLUMN IF NOT EXISTS ai_chat_summary_ko_id TEXT NULL;

COMMENT ON COLUMN health_data_tcr_event.ai_chat_summary_ko_id IS
    'Optional KO id linking this event to a hash-only AiChatSummaryV1 row in health_data_tcr_ai_chat_summary_v1.';

-- Foreign key is soft (ON DELETE SET NULL) to avoid hard coupling of TCR events
-- to AiChatSummaryV1 lifecycles; adjust if you prefer CASCADE.
ALTER TABLE IF EXISTS health_data_tcr_event
    ADD CONSTRAINT fk_health_data_tcr_event_ai_chat_summary
        FOREIGN KEY (tcr_id, ai_chat_summary_ko_id)
        REFERENCES health_data_tcr_ai_chat_summary_v1 (tcr_id, ko_id)
        ON DELETE SET NULL;

COMMIT;
