-- filename: dbinput_sovereign_check_phoenix.sql
-- destination: ecorestorationshard/db/dbinput_sovereign_check_phoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 1. Register the SQL input file in repofile.

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
    'db/dbinput_sovereign_check_phoenix.sql',
    'SQL_SCHEMA',
    'SQL',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 2. DefinitionRegistry registration for the engineered input.

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
    'input.sovereign_check.phoenix.2026v1'          AS logicalname,
    '2026v1'                                        AS versiontag,
    '0x' || lower(hex(randomblob(16)))             AS hash,
    'ACTIVE'                                        AS status,
    r.repoid                                        AS repoid,
    'db/dbinput_sovereign_check_phoenix.sql'        AS relpathsql,
    NULL                                            AS relpathaln,
    NULL                                            AS relpathdoc,
    'AGENT_PROMPT'                                  AS ecoscope,
    'SovereignCheckPhoenix2026v1'                   AS contractid,
    '2026-01-01T00:00:00Z'                          AS createdutc,
    '2026-01-01T00:00:00Z'                          AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 3. Identity binding: engineered input owned by primary Bostrom address.

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
    'input.sovereign_check.phoenix.2026v1',
    'github.commk-bluebirdecorestorationshard',
    'db/dbinput_sovereign_check_phoenix.sql',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCE_DB',
    'SovereignCheckPhoenix2026v1',
    'Engineered pre-flight input verifying psych_risk_consent metadata and prohibiting weaponized influence for this Bostrom identity.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);
