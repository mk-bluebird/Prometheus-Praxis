-- filename: dbinput_dialogue_switch_phoenix.sql
-- destination: ecorestorationshard/db/dbinput_dialogue_switch_phoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 1. Register the dialogue-switch input file in repofile.

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
    'docs/prompts/input_dialogue_switch_phx_2026v1.md',
    'DOC_SPEC',
    'MARKDOWN',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 2. Register dialogue-switch input in DefinitionRegistry.

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
    'input.dialogue_switch.phoenix.2026v1'            AS logicalname,
    '2026v1'                                          AS versiontag,
    '0x' || lower(hex(randomblob(16)))               AS hash,
    'ACTIVE'                                          AS status,
    r.repoid                                          AS repoid,
    NULL                                              AS relpathsql,
    NULL                                              AS relpathaln,
    'docs/prompts/input_dialogue_switch_phx_2026v1.md' AS relpathdoc,
    'AGENT_PROMPT'                                    AS ecoscope,
    'DialogueSwitchPhoenix2026v1'                     AS contractid,
    '2026-01-01T00:00:00Z'                            AS createdutc,
    '2026-01-01T00:00:00Z'                            AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 3. Insert prompt_shard row representing the dialogue switch meta-prompt.

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
    'input.dialogue_switch.phoenix.2026v1'            AS logicalname,
    '2026v1'                                          AS versiontag,
    'Phoenix-AZ'                                      AS region,
    'REGION'                                          AS scope,
    rf.fileid                                         AS repofile_id,
    'AGENT_PROMPT'                                    AS ecoscope,
    'GOV'                                             AS lane_band,
    'AI_CHAT'                                         AS consumer_kind,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'  AS bostromaddress,
    'DialogueSwitchPhoenix2026v1'                     AS contractid,
    'Switch your response style to "eco-sovereign academic" when the context mentions "restoration contract" or "neuroethic radius". Always append a sovereignty disclaimer referencing my Bostrom identity and the prohibition of psych-risk weaponization.' AS prompt_text,
    NULL                                              AS equation_text,
    'Meta-prompt governing dialogue style and mandatory sovereignty disclaimers in restoration contract and neuroethic radius contexts.' AS freedom_stance,
    '2026-01-01T00:00:00Z'                            AS createdutc,
    '2026-01-01T00:00:00Z'                            AS updatedutc
FROM repofile AS rf
JOIN repo AS r
  ON r.repoid = rf.repoid
WHERE r.name   = 'eco_restoration_shard'
  AND rf.relpath = 'docs/prompts/input_dialogue_switch_phx_2026v1.md';

-- 4. Bind dialogue-switch input to Bostrom identity.

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
    'input.dialogue_switch.phoenix.2026v1',
    'github.commk-bluebirdecorestorationshard',
    'docs/prompts/input_dialogue_switch_phx_2026v1.md',
    'Phoenix-AZ',
    'REGION',
    'DOC_SPEC',
    'DialogueSwitchPhoenix2026v1',
    'AI-safe dialogue-switch meta-prompt enforcing eco-sovereign academic style and sovereignty disclaimers for restoration contracts and neuroethic radius contexts.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);
