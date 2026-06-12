-- filename: db/eco_constellation_knowledge_index.sql
-- destination: eco_restoration_shard/db/eco_constellation_knowledge_index.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Core table: eco_knowledge_shard_index
--    Single index for ALN (.aln), SAI (.sai), and MAI (.mai) specs.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_knowledge_shard_index (
    id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    shard_name         TEXT    NOT NULL,  -- e.g. eco_lake_ker_governance_v1.aln
    shard_type         TEXT    NOT NULL,  -- 'ALN' | 'SAI' | 'MAI'
    schema_id          TEXT    NOT NULL,  -- e.g. ALN-ECO-LAKE-KER-GOV-1
    version            TEXT    NOT NULL,  -- semantic version, e.g. 1.0.0
    filename           TEXT    NOT NULL,  -- repo‑relative path, e.g. knowledge/eco_lake_ker_governance_v1.aln
    scope              TEXT    NOT NULL,  -- high‑level scope string
    repo_target        TEXT    NOT NULL,  -- target repository slug, e.g. eco_restoration_shard
    description        TEXT    NOT NULL,  -- short summary, no markdown
    author_did         TEXT    NOT NULL,  -- DID of author
    created_at         TEXT    NOT NULL,  -- ISO8601 string
    bound_schemas      TEXT    NOT NULL,  -- CSV of bound DB schemas or tables
    primary_did        TEXT    NOT NULL,  -- IDENTITY.PRIMARY_DID where applicable
    alt_did            TEXT    NOT NULL,  -- IDENTITY.ALT_DID where applicable
    wallet_evm         TEXT    NOT NULL,  -- IDENTITY.WALLET_EVM where applicable
    facebook_profile   TEXT    NOT NULL,  -- IDENTITY.FACEBOOK_PROFILE_URL where applicable
    ker_axis_tag       TEXT    NOT NULL,  -- tag describing K/E/R axis usage, e.g. 'lake_ker'
    safety_profile     TEXT    NOT NULL,  -- e.g. 'no_personal_reid', 'no_private_inference'
    active             INTEGER NOT NULL DEFAULT 1, -- 1 = active, 0 = superseded
    UNIQUE (schema_id, version)
);

CREATE INDEX IF NOT EXISTS idx_eco_knowledge_shard_type
    ON eco_knowledge_shard_index (shard_type);

CREATE INDEX IF NOT EXISTS idx_eco_knowledge_shard_scope
    ON eco_knowledge_shard_index (scope);

CREATE INDEX IF NOT EXISTS idx_eco_knowledge_shard_repo
    ON eco_knowledge_shard_index (repo_target);

----------------------------------------------------------------------
-- 2. Table: eco_repo_pathmap
--    Maps logical repo targets + filenames to physical destinations.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_repo_pathmap (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_target   TEXT    NOT NULL,  -- e.g. eco_restoration_shard
    filename      TEXT    NOT NULL,  -- same as eco_knowledge_shard_index.filename
    dest_path     TEXT    NOT NULL,  -- POSIX path within repo checkout
    lane_scope    TEXT    NOT NULL,  -- e.g. 'RESEARCH', 'EXPPROD', 'PROD'
    active        INTEGER NOT NULL DEFAULT 1,
    UNIQUE (repo_target, filename)
);

CREATE INDEX IF NOT EXISTS idx_eco_repo_pathmap_repo
    ON eco_repo_pathmap (repo_target);

----------------------------------------------------------------------
-- 3. Seed rows: three lake governance / social signal / human loop specs
--    These INSERT statements are idempotent via INSERT OR REPLACE.
--    Adjust created_at timestamps as needed during actual instantiation.
----------------------------------------------------------------------

INSERT OR REPLACE INTO eco_knowledge_shard_index (
    id,
    shard_name,
    shard_type,
    schema_id,
    version,
    filename,
    scope,
    repo_target,
    description,
    author_did,
    created_at,
    bound_schemas,
    primary_did,
    alt_did,
    wallet_evm,
    facebook_profile,
    ker_axis_tag,
    safety_profile,
    active
) VALUES
-- 1) ALN: eco_lake_ker_governance_v1.aln
(
    NULL,
    'eco_lake_ker_governance_v1.aln',
    'ALN',
    'ALN-ECO-LAKE-KER-GOV-1',
    '1.0.0',
    'knowledge/eco_lake_ker_governance_v1.aln',
    'global_lake_and_watershed_risk_governance',
    'eco_restoration_shard',
    'Normative K/E/R governance shard for global lake and watershed risk reasoning.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    '2026-06-12T00:00:00Z',
    'eco_lake_risk_postgres,eco_lake_risk_sqlite',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc',
    '0x519fC0eB4111323Cac44b70e1aE31c30e405802D',
    'https://facebook.com/profile.php?id=61583146843874',
    'lake_ker',
    'no_personal_reid_no_private_inference',
    1
),
-- 2) SAI: eco_social_signal_extraction_v1.sai
(
    NULL,
    'eco_social_signal_extraction_v1.sai',
    'SAI',
    'SAI-ECO-SOCIAL-SIGNAL-1',
    '1.0.0',
    'knowledge/eco_social_signal_extraction_v1.sai',
    'social_media_and_citizen_reports_for_lake_events',
    'eco_restoration_shard',
    'Signal-processing fragment for extracting social and citizen reports into social_signal entries.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    '2026-06-12T00:00:00Z',
    'eco_lake_risk_postgres,eco_lake_risk_sqlite',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc',
    '0x519fC0eB4111323Cac44b70e1aE31c30e405802D',
    'https://facebook.com/profile.php?id=61583146843874',
    'lake_social_signal_K_channel',
    'no_personal_reid_no_private_inference',
    1
),
-- 3) MAI: eco_risk_human_review_loop_v1.mai
(
    NULL,
    'eco_risk_human_review_loop_v1.mai',
    'MAI',
    'MAI-ECO-RISK-HUMAN-LOOP-1',
    '1.0.0',
    'workflows/eco_risk_human_review_loop_v1.mai',
    'periodic_and_event_driven_lake_risk_assessment_and_alerts',
    'eco_restoration_shard',
    'Execution particle for human-in-the-loop lake risk assessment, governance events, and alerts.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    '2026-06-12T00:00:00Z',
    'eco_lake_risk_postgres,eco_lake_risk_sqlite',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc',
    '0x519fC0eB4111323Cac44b70e1aE31c30e405802D',
    'https://facebook.com/profile.php?id=61583146843874',
    'lake_human_loop_R_guardrail',
    'no_personal_reid_no_private_inference',
    1
);

----------------------------------------------------------------------
-- 4. Path map seeds: map logical filenames into repo destinations.
----------------------------------------------------------------------

INSERT OR REPLACE INTO eco_repo_pathmap (
    id,
    repo_target,
    filename,
    dest_path,
    lane_scope,
    active
) VALUES
(
    NULL,
    'eco_restoration_shard',
    'knowledge/eco_lake_ker_governance_v1.aln',
    'knowledge/eco_lake_ker_governance_v1.aln',
    'GOVERNANCE',
    1
),
(
    NULL,
    'eco_restoration_shard',
    'knowledge/eco_social_signal_extraction_v1.sai',
    'knowledge/eco_social_signal_extraction_v1.sai',
    'GOVERNANCE',
    1
),
(
    NULL,
    'eco_restoration_shard',
    'workflows/eco_risk_human_review_loop_v1.mai',
    'workflows/eco_risk_human_review_loop_v1.mai',
    'GOVERNANCE',
    1
);
