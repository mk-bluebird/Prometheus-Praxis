-- filename: dbpromptshard_insert_sovereign_governance.sql
-- destination: ecorestorationshard/db/dbpromptshard_insert_sovereign_governance.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- Assumptions:
--  - The prompt text is stored in a Markdown or text file, e.g.
--      docs/prompts/prompt_sovereign_governance_phx_2026v1.md
--  - This file is registered in repofile with purpose = 'DOC_SPEC' and language = 'MARKDOWN'.

-- 1. Register prompt file in repofile (if not already present).

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
    'docs/prompts/prompt_sovereign_gov_phx_2026v1.md',
    'DOC_SPEC',
    'MARKDOWN',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 2. Register prompt in DefinitionRegistry slice.

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
    'prompt.sovereign.governance.phoenix.2026v1' AS logicalname,
    '2026v1'                                     AS versiontag,
    '0x' || lower(hex(randomblob(16)))          AS hash,
    'ACTIVE'                                     AS status,
    r.repoid                                     AS repoid,
    NULL                                         AS relpathsql,
    NULL                                         AS relpathaln,
    'docs/prompts/prompt_sovereign_gov_phx_2026v1.md' AS relpathdoc,
    'AGENT_PROMPT'                               AS ecoscope,
    'SovereignGovernancePhoenix2026v1'           AS contractid,
    '2026-01-01T00:00:00Z'                       AS createdutc,
    '2026-01-01T00:00:00Z'                       AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 3. Insert prompt shard row bound to primary Bostrom address.

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
    'prompt.sovereign.governance.phoenix.2026v1'       AS logicalname,
    '2026v1'                                           AS versiontag,
    'Phoenix-AZ'                                       AS region,
    'REGION'                                           AS scope,
    rf.fileid                                          AS repofile_id,
    'AGENT_PROMPT'                                     AS ecoscope,
    'GOV'                                              AS lane_band,
    'AI_CHAT'                                          AS consumer_kind,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'   AS bostromaddress,
    'SovereignGovernancePhoenix2026v1'                 AS contractid,
    'You are a non-actuating governance co-pilot bound to bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7. All queries must reference the sovereign restoration spine and reject any input traceable to non-consensual psych-risk instruments. Based on vrestorationnodesphx, propose 5 eco-restoration actions that maximize deltakarmawindow while minimizing gwriskmax.' AS prompt_text,
    'eta = Delta_karma / (Delta_mass + 1)'             AS equation_text,
    'Explicitly forbids non-consensual, MK-Ultra-style influence and enforces a sovereign, neurorights-respecting filter over all dialogue.' AS freedom_stance,
    '2026-01-01T00:00:00Z'                             AS createdutc,
    '2026-01-01T00:00:00Z'                             AS updatedutc
FROM repofile AS rf
JOIN repo AS r
  ON r.repoid = rf.repoid
WHERE r.name   = 'eco_restoration_shard'
  AND rf.relpath = 'docs/prompts/prompt_sovereign_gov_phx_2026v1.md';

-- 4. Bind prompt to Bostrom identity in restorationidentitybinding.

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
    'prompt.sovereign.governance.phoenix.2026v1',
    'github.commk-bluebirdecorestorationshard',
    'docs/prompts/prompt_sovereign_gov_phx_2026v1.md',
    'Phoenix-AZ',
    'REGION',
    'DOC_SPEC',
    'SovereignGovernancePhoenix2026v1',
    'Sovereign, neurorights-respecting AI-chat prompt for eco-governance, bound to primary Bostrom address.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);
