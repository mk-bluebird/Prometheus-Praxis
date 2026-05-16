-- filename: insert_db_ecoperjoule_policy_energy.sql
-- destination: ecorestorationshard/db/insert_db_ecoperjoule_policy_energy.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 26. Register db/db_ecoperjoule_policy_energy.sql in repofile
INSERT OR IGNORE INTO repofile (
    repoid,
    relpath,
    purpose,
    language
)
SELECT
    r.repoid,
    'db/db_ecoperjoule_policy_energy.sql',
    'Eco-per-joule policy grammar for Phoenix energy planes',
    'SQL'
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- 26. Register in definitionregistryrestoration
INSERT OR IGNORE INTO definitionregistryrestoration (
    logicalname,
    versiontag,
    hash,
    status,
    contractid,
    repofileid
)
SELECT
    'energy.ecoperjoule.policy.phoenix.2026v1'      AS logicalname,
    '2026v1'                                        AS versiontag,
    '0x' || lower(hex(randomblob(16)))             AS hash,         -- placeholder hash; replace with canonical content hash in CI
    'ACTIVE'                                        AS status,
    'EcoplaneEnergyPhoenix2026v1'                   AS contractid,
    rf.fileid                                       AS repofileid
FROM repofile AS rf
JOIN repo AS r
  ON r.repoid = rf.repoid
WHERE r.name   = 'eco_restoration_shard'
  AND rf.relpath = 'db/db_ecoperjoule_policy_energy.sql';

-- 26. Bind to primary Bostrom address in restorationidentitybinding
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
    'energy.ecoperjoule.policy.phoenix.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/db_ecoperjoule_policy_energy.sql',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCE',
    'EcoplaneEnergyPhoenix2026v1',
    'Author of eco-per-joule policy and governance views for Phoenix energy planes.',
    datetime('now'),
    datetime('now')
);
