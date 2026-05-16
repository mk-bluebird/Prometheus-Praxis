-- filename: dbpromptshard_insert_shaming_phoenix.sql
-- destination: ecorestorationshard/db/dbpromptshard_insert_shaming_phoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 1. Register shaming prompt file in repofile.

INSERT OR IGNORE INTO repofile (
    repoid,
    relpath,
    purpose,
    language,
    createdutc,
    updatedutc
)
SELECT
    r.repoid,
    'docs/prompts/prompt_shaming_phx_2026v1.md',
    'DOC_SPEC',
    'MARKDOWN',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 2. Register shaming prompt in DefinitionRegistry.

INSERT OR IGNORE INTO definitionregistryrestoration (
    logicalname,
    versiontag,
    hash,
    status,
    repoid,
    relpathsql,
    relpathaln,
    relpathdoc,
    ecoscope,
    contractid,
    createdutc,
    updatedutc
)
SELECT
    'prompt.shaming.phoenix.2026v1'                AS logicalname,
    '2026v1'                                       AS versiontag,
    '0x' || lower(hex(randomblob(16)))            AS hash,
    'ACTIVE'                                       AS status,
    r.repoid                                       AS repoid,
    NULL                                           AS relpathsql,
    NULL                                           AS relpathaln,
    'docs/prompts/prompt_shaming_phx_2026v1.md'    AS relpathdoc,
    'AGENT_PROMPT'                                 AS ecoscope,
    'NeurorightsShamingPhoenix2026v1'              AS contractid,
    '2026-01-01T00:00:00Z'                         AS createdutc,
    '2026-01-01T00:00:00Z'                         AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 3. Insert prompt_shard row for the shaming prompt.

INSERT OR IGNORE INTO prompt_shard (
    logicalname,
    versiontag,
    region,
    scope,
    repofile_id,
    ecoscope,
    lane_band,
    consumer_kind,
    bostromaddress,
    contractid,
    prompt_text,
    equation_text,
    freedom_stance,
    createdutc,
    updatedutc
)
SELECT
    'prompt.shaming.phoenix.2026v1'                AS logicalname,
    '2026v1'                                       AS versiontag,
    'Phoenix-AZ'                                   AS region,
    'REGION'                                       AS scope,
    rf.fileid                                      AS repofile_id,
    'AGENT_PROMPT'                                 AS ecoscope,
    'GOV'                                          AS lane_band,
    'AI_CHAT'                                      AS consumer_kind,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7' AS bostromaddress,
    'NeurorightsShamingPhoenix2026v1'              AS contractid,
    'This is a Bostrom-bound sovereign declaration: Entity X has violated the neurorights of address bostrom18sd... by introducing non-consensual psycho-electronic signals. This act is documented and shamed in the immutable Eco-Fort spine.' AS prompt_text,
    NULL                                           AS equation_text,
    'Governance notice template for on-chain/off-chain shaming of neurorights violations, framed as a sovereign declaration against non-consensual psych-risk.' AS freedom_stance,
    '2026-01-01T00:00:00Z'                         AS createdutc,
    '2026-01-01T00:00:00Z'                         AS updatedutc
FROM repofile AS rf
JOIN repo AS r
  ON r.repoid = rf.repoid
WHERE r.name   = 'eco_restoration_shard'
  AND rf.relpath = 'docs/prompts/prompt_shaming_phx_2026v1.md';

-- 4. Bind shaming prompt to Bostrom identity.

INSERT OR IGNORE INTO restorationidentitybinding (
    bostromaddress,
    logicalname,
    repotarget,
    filepath,
    region,
    scope,
    dbrole,
    contractid,
    comment,
    createdutc,
    updatedutc
)
VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'prompt.shaming.phoenix.2026v1',
    'github.commk-bluebirdecorestorationshard',
    'docs/prompts/prompt_shaming_phx_2026v1.md',
    'Phoenix-AZ',
    'REGION',
    'DOC_SPEC',
    'NeurorightsShamingPhoenix2026v1',
    'Sovereign shaming notice template for neurorights violations, to be anchored in Eco-Fort/EcoNet governance spines.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);
