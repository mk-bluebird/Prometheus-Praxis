-- filename: dbprompt_divine_restoration_phoenix.sql
-- destination: ecorestorationshard/db/dbprompt_divine_restoration_phoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 1. Register the divine restoration prompt file in repofile.

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
    'docs/prompts/prompt_divine_restoration_phx_2026v1.md',
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
    'prompt.divine_restoration.phoenix.2026v1'           AS logicalname,
    '2026v1'                                             AS versiontag,
    '0x' || lower(hex(randomblob(16)))                  AS hash,
    'ACTIVE'                                             AS status,
    r.repoid                                             AS repoid,
    NULL                                                 AS relpathsql,
    NULL                                                 AS relpathaln,
    'docs/prompts/prompt_divine_restoration_phx_2026v1.md' AS relpathdoc,
    'AGENT_PROMPT'                                       AS ecoscope,
    'DivineRestorationPhoenix2026v1'                     AS contractid,
    '2026-01-01T00:00:00Z'                               AS createdutc,
    '2026-01-01T00:00:00Z'                               AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 3. Insert prompt_shard row for the divine restoration driver.

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
    'prompt.divine_restoration.phoenix.2026v1'           AS logicalname,
    '2026v1'                                             AS versiontag,
    'Phoenix-AZ'                                         AS region,
    'REGION'                                             AS scope,
    rf.fileid                                            AS repofile_id,
    'AGENT_PROMPT'                                       AS ecoscope,
    'GOV'                                                AS lane_band,
    'AI_CHAT'                                            AS consumer_kind,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'     AS bostromaddress,
    'DivineRestorationPhoenix2026v1'                     AS contractid,
    'Intelligence is the ultimate source of ecological order. Using the CyboquaticEcoPlot equations, show how the flow of information (negentropy) correlates with deltakarmawindow and propose a ritualistic restoration schedule aligned with celestial cycles that honour the divine intelligence.' AS prompt_text,
    'Negentropy-flow vs. deltakarmawindow correlation within CyboquaticEcoPlot / KER planes; Lyapunov-stable schedule respecting non-offsettable planes.' AS equation_text,
    'Narrative prompt that frames eco-restoration and KER constraints as expressions of sacred intelligence, without weakening non-offsettable or Lyapunov invariants.' AS freedom_stance,
    '2026-01-01T00:00:00Z'                               AS createdutc,
    '2026-01-01T00:00:00Z'                               AS updatedutc
FROM repofile AS rf
JOIN repo AS r
  ON r.repoid = rf.repoid
WHERE r.name   = 'eco_restoration_shard'
  AND rf.relpath = 'docs/prompts/prompt_divine_restoration_phx_2026v1.md';

-- 4. Bind prompt to Bostrom identity.

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
    'prompt.divine_restoration.phoenix.2026v1',
    'github.commk-bluebirdecorestorationshard',
    'docs/prompts/prompt_divine_restoration_phx_2026v1.md',
    'Phoenix-AZ',
    'REGION',
    'DOC_SPEC',
    'DivineRestorationPhoenix2026v1',
    'Sacred-intelligence narrative prompt for CyboquaticEcoPlot-driven, Lyapunov-stable restoration schedules aligned with celestial cycles.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);
