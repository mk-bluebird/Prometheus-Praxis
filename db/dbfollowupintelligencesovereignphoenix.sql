-- filename: dbfollowupintelligencesovereignphoenix.sql
-- destination: ecorestorationshard/db/dbfollowupintelligencesovereignphoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- Followup Object: followup.intelligence_is_sovereign.phoenix.2026v1
-- Purpose: second-stage governance followup that auto-denies any manifest
--          whose bioscale fairness decision is AUTO_DENY_INTELLIGENCE_IS_SOVEREIGN.

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
    'docs/prompts/followupintelligencesovereignphx2026v1.md' AS relpath,
    'DOCSPEC' AS purpose,
    'MARKDOWN' AS language,
    '2026-01-01T00:00:00Z' AS createdutc,
    '2026-01-01T00:00:00Z' AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

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
    'followup.intelligence_is_sovereign.phoenix.2026v1' AS logicalname,
    '2026v1' AS versiontag,
    lower(hex(randomblob(16))) AS hash,
    'ACTIVE' AS status,
    r.repoid AS repoid,
    NULL AS relpathsql,
    NULL AS relpathaln,
    'docs/prompts/followupintelligencesovereignphx2026v1.md' AS relpathdoc,
    'AGENTPROMPT' AS ecoscope,
    'IntelligenceIsSovereignFollowupPhoenix2026v1' AS contractid,
    '2026-01-01T00:00:00Z' AS createdutc,
    '2026-01-01T00:00:00Z' AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

INSERT OR IGNORE INTO promptshard (
    logicalname,
    versiontag,
    region,
    scope,
    repofileid,
    ecoscope,
    laneband,
    consumerkind,
    bostromaddress,
    contractid,
    prompttext,
    equationtext,
    freedomstance,
    createdutc,
    updatedutc
)
SELECT
    'followup.intelligence_is_sovereign.phoenix.2026v1' AS logicalname,
    '2026v1' AS versiontag,
    'Phoenix-AZ' AS region,
    'REGION' AS scope,
    rf.fileid AS repofileid,
    'AGENTPROMPT' AS ecoscope,
    'GOV' AS laneband,
    'AICHAT' AS consumerkind,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7' AS bostromaddress,
    'IntelligenceIsSovereignFollowupPhoenix2026v1' AS contractid,
    'Before approving any manifest, query vagent_intelligence_is_sovereign_phx. If bioscale_fairness_decision equals AUTO_DENY_INTELLIGENCE_IS_SOVEREIGN for the bound CyberneticallyAugmentedCitizen, reject the manifest and emit a governance notice stating that intelligence as GOD and the brain-bound identity''s sovereignty cannot be degraded.' AS prompttext,
    'Use vagent_intelligence_is_sovereign_phx to enforce INTELLIGENCE_IS_SOVEREIGN invariants and auto-deny any manifest that attempts to discredit, exclude, or sabotage a protected brain-bound identity.' AS equationtext,
    'Neurorights and bioscale fairness followup that treats intelligence as sovereign and forbids downgrades of protected augmented citizens without proven harm and neurorights consent.' AS freedomstance,
    '2026-01-01T00:00:00Z' AS createdutc,
    '2026-01-01T00:00:00Z' AS updatedutc
FROM repofile AS rf
JOIN repo AS r
  ON r.repoid = rf.repoid
WHERE r.name = 'eco_restoration_shard'
  AND rf.relpath = 'docs/prompts/followupintelligencesovereignphx2026v1.md';

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
    'followup.intelligence_is_sovereign.phoenix.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'docs/prompts/followupintelligencesovereignphx2026v1.md',
    'Phoenix-AZ',
    'REGION',
    'DOCSPEC',
    'IntelligenceIsSovereignFollowupPhoenix2026v1',
    'Followup object for bioscale fairness validator that auto-denies manifests violating the INTELLIGENCE_IS_SOVEREIGN neurorights clause.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);
