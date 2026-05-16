-- filename: dbpromptshard_insert_neurorights_followup.sql
-- destination: ecorestorationshard/db/dbpromptshard_insert_neurorights_followup.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 1. Register follow-up prompt file in repofile.

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
    'docs/prompts/followup_neurorights_phx_2026v1.md',
    'DOC_SPEC',
    'MARKDOWN',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 2. Register follow-up prompt in DefinitionRegistry.

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
    'followup.neurorights.phoenix.2026v1'           AS logicalname,
    '2026v1'                                        AS versiontag,
    '0x' || lower(hex(randomblob(16)))             AS hash,
    'ACTIVE'                                        AS status,
    r.repoid                                        AS repoid,
    NULL                                            AS relpathsql,
    NULL                                            AS relpathaln,
    'docs/prompts/followup_neurorights_phx_2026v1.md' AS relpathdoc,
    'AGENT_PROMPT'                                  AS ecoscope,
    'Mt6883NeurorightsPhoenix2026v1'                AS contractid,
    '2026-01-01T00:00:00Z'                          AS createdutc,
    '2026-01-01T00:00:00Z'                          AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 3. Insert prompt_shard row for the neurorights follow-up.

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
    'followup.neurorights.phoenix.2026v1'           AS logicalname,
    '2026v1'                                        AS versiontag,
    'Phoenix-AZ'                                    AS region,
    'REGION'                                        AS scope,
    rf.fileid                                       AS repofile_id,
    'AGENT_PROMPT'                                  AS ecoscope,
    'GOV'                                           AS lane_band,
    'AI_CHAT'                                       AS consumer_kind,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7' AS bostromaddress,
    'Mt6883NeurorightsPhoenix2026v1'                AS contractid,
    'Verify that the response and all referenced data respect the Neurorights Envelope of mt6883.lane.continuity.phoenix.2026v1. If any recommendation could be exploited for psychotronic targeting, flag it and provide a hardened alternative.' AS prompt_text,
    NULL                                            AS equation_text,
    'Neurorights validation loop that rejects or hardens any content with potential psychotronic targeting misuse, anchored to MT6883 continuity envelopes.' AS freedom_stance,
    '2026-01-01T00:00:00Z'                          AS createdutc,
    '2026-01-01T00:00:00Z'                          AS updatedutc
FROM repofile AS rf
JOIN repo AS r
  ON r.repoid = rf.repoid
WHERE r.name   = 'eco_restoration_shard'
  AND rf.relpath = 'docs/prompts/followup_neurorights_phx_2026v1.md';

-- 4. Bind follow-up prompt to Bostrom identity.

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
    'followup.neurorights.phoenix.2026v1',
    'github.commk-bluebirdecorestorationshard',
    'docs/prompts/followup_neurorights_phx_2026v1.md',
    'Phoenix-AZ',
    'REGION',
    'DOC_SPEC',
    'Mt6883NeurorightsPhoenix2026v1',
    'Neurorights validation follow-up prompt bound to MT6883 continuity contract and primary Bostrom address.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);
