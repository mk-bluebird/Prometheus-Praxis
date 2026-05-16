-- filename: dbpromptshard_insert_zk_psych_risk_phoenix.sql
-- destination: ecorestorationshard/db/dbpromptshard_insert_zk_psych_risk_phoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 1. Register the ZK prompt file in repofile.

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
    'docs/prompts/prompt_zk_psych_risk_phx_2026v1.md',
    'DOC_SPEC',
    'MARKDOWN',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 2. Register ZK prompt in DefinitionRegistry.

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
    'prompt.zk_psych_risk.phoenix.2026v1'          AS logicalname,
    '2026v1'                                       AS versiontag,
    '0x' || lower(hex(randomblob(16)))            AS hash,
    'ACTIVE'                                       AS status,
    r.repoid                                       AS repoid,
    NULL                                           AS relpathsql,
    NULL                                           AS relpathaln,
    'docs/prompts/prompt_zk_psych_risk_phx_2026v1.md' AS relpathdoc,
    'AGENT_PROMPT'                                 AS ecoscope,
    'PsychRiskFortressPhoenix2026v1'               AS contractid,
    '2026-01-01T00:00:00Z'                         AS createdutc,
    '2026-01-01T00:00:00Z'                         AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 3. Insert prompt_shard row for the ZK psych-risk prompt.

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
    'prompt.zk_psych_risk.phoenix.2026v1'          AS logicalname,
    '2026v1'                                       AS versiontag,
    'Phoenix-AZ'                                   AS region,
    'REGION'                                       AS scope,
    rf.fileid                                      AS repofile_id,
    'AGENT_PROMPT'                                 AS ecoscope,
    'GOV'                                          AS lane_band,
    'AI_CHAT'                                      AS consumer_kind,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7' AS bostromaddress,
    'PsychRiskFortressPhoenix2026v1'               AS contractid,
    'Design a ZK-circuit that proves a restoration contract remains free of non-consensual psych-risk influence, with public inputs being the contract''s Merkle root and my Bostrom identity. The circuit must be named psych_risk_fortress_2026.' AS prompt_text,
    'Public inputs: contract Merkle root, Bostrom identity; public output: commitment to KER metrics and a zero-knowledge proof that no external psych-risk vector altered the decision.' AS equation_text,
    'ZK design prompt enforcing neurorights: proves KER-compliant, psych-risk-free governance without exposing sensitive restoration or identity data.' AS freedom_stance,
    '2026-01-01T00:00:00Z'                         AS createdutc,
    '2026-01-01T00:00:00Z'                         AS updatedutc
FROM repofile AS rf
JOIN repo AS r
  ON r.repoid = rf.repoid
WHERE r.name   = 'eco_restoration_shard'
  AND rf.relpath = 'docs/prompts/prompt_zk_psych_risk_phx_2026v1.md';

-- 4. Bind ZK prompt to Bostrom identity in restorationidentitybinding.

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
    'prompt.zk_psych_risk.phoenix.2026v1',
    'github.commk-bluebirdecorestorationshard',
    'docs/prompts/prompt_zk_psych_risk_phx_2026v1.md',
    'Phoenix-AZ',
    'REGION',
    'DOC_SPEC',
    'PsychRiskFortressPhoenix2026v1',
    'ZK psych-risk fortress prompt for contracts, ensuring neurorights-respecting proofs bound to primary Bostrom identity.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);
