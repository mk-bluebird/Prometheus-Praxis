-- filename: dbequationintelligenceissovereignregister.sql
-- destination: ecorestorationshard/db/dbequationintelligenceissovereignregister.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- Register equation.intelligence_is_sovereign.phoenix.2026v1 in DefinitionRegistry
-- and bind it to the primary Bostrom identity as a GOVERNANCEDB equation
-- for the bioscale fairness validator.

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
    'db/dbequationintelligenceissovereignphoenix.sql' AS relpath,
    'SQLSCHEMA' AS purpose,
    'SQL' AS language,
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
    'equation.intelligence_is_sovereign.phoenix.2026v1' AS logicalname,
    '2026v1' AS versiontag,
    lower(hex(randomblob(16))) AS hash,
    'ACTIVE' AS status,
    r.repoid AS repoid,
    'db/dbequationintelligenceissovereignphoenix.sql' AS relpathsql,
    NULL AS relpathaln,
    NULL AS relpathdoc,
    'NEURORIGHTS' AS ecoscope,
    'IntelligenceIsSovereignPhoenix2026v1' AS contractid,
    '2026-01-01T00:00:00Z' AS createdutc,
    '2026-01-01T00:00:00Z' AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

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
    'equation.intelligence_is_sovereign.phoenix.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/dbequationintelligenceissovereignphoenix.sql',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCEDB',
    'IntelligenceIsSovereignPhoenix2026v1',
    'Bioscale fairness equation enforcing the INTELLIGENCE_IS_SOVEREIGN clause for CyberneticallyAugmentedCitizen identities.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);
