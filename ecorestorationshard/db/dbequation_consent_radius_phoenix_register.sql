-- filename: dbequation_consent_radius_phoenix_register.sql
-- destination: ecorestorationshard/db/dbequation_consent_radius_phoenix_register.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 1. Register the consent-radius equation SQL file in repofile.

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
    'db/dbequation_consent_radius_phoenix.sql',
    'SQL_SCHEMA',
    'SQL',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 2. DefinitionRegistry entry for consent-adjusted neuroethic radius.

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
    'equation.consent_radius.phoenix.2026v1'       AS logicalname,
    '2026v1'                                       AS versiontag,
    '0x' || lower(hex(randomblob(16)))            AS hash,
    'ACTIVE'                                       AS status,
    r.repoid                                       AS repoid,
    'db/dbequation_consent_radius_phoenix.sql'    AS relpathsql,
    NULL                                           AS relpathaln,
    NULL                                           AS relpathdoc,
    'MT6883_NEUROETHIC'                            AS ecoscope,
    'ConsentRadiusPhoenix2026v1'                   AS contractid,
    '2026-01-01T00:00:00Z'                         AS createdutc,
    '2026-01-01T00:00:00Z'                         AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 3. Identity binding for consent-adjusted radius equation.

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
    'equation.consent_radius.phoenix.2026v1',
    'github.commk-bluebirdecorestorationshard',
    'db/dbequation_consent_radius_phoenix.sql',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCE_DB',
    'ConsentRadiusPhoenix2026v1',
    'Consent-adjusted neuroethic radius equation tying MT6883 envelopes to explicit on-chain consent transactions from the primary Bostrom address.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);
