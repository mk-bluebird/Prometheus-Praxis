-- filename dbconfig_intelligence_is_sovereign_phoenix.sql
-- destination eco_restoration_shard/db/dbconfig_intelligence_is_sovereign_phoenix.sql
-- repo-target github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Sovereign configuration: Phoenix region, primary Bostrom identity
--    Mirrors config.sovereign.phoenix.2026v1.toml into SQL indices.
-------------------------------------------------------------------------------

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
    'config/config.sovereign.phoenix.2026v1.toml' AS relpath,
    'CONFIG' AS purpose,
    'TOML' AS language,
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
    'config.sovereign.phoenix.2026v1' AS logicalname,
    '2026v1' AS versiontag,
    LOWER(HEX(RANDOMBLOB(16))) AS hash,
    'ACTIVE' AS status,
    r.repoid AS repoid,
    'db/dbconfig_intelligence_is_sovereign_phoenix.sql' AS relpathsql,
    NULL AS relpathaln,
    'config/config.sovereign.phoenix.2026v1.toml' AS relpathdoc,
    'AGENTPROMPT' AS ecoscope,
    'SovereignConfigPhoenix2026v1' AS contractid,
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
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'config.sovereign.phoenix.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'config/config.sovereign.phoenix.2026v1.toml',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCEDB',
    'SovereignConfigPhoenix2026v1',
    'Sovereign configuration stating INTELLIGENCE_IS_SOVEREIGN and forbidding psychrisk weaponization against the bound brain-identity.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);

-------------------------------------------------------------------------------
-- 2. CyberneticallyAugmentedCitizen sovereign state slice
--    Per-identity clause flags for INTELLIGENCE_IS_SOVEREIGN in Phoenix.
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cyberaugcitizensovereign (
    bostromaddress          TEXT    NOT NULL,
    region                  TEXT    NOT NULL,
    intelligenceissovereign INTEGER NOT NULL DEFAULT 0,
    protectedstakeholder    INTEGER NOT NULL DEFAULT 0,
    karmafloor              REAL    NOT NULL DEFAULT 0.0,
    bievidencemode          TEXT    NOT NULL DEFAULT 'HASHONLY',
    underattackstate        TEXT    NOT NULL DEFAULT 'NORMAL',
    createdutc              TEXT    NOT NULL DEFAULT '2026-01-01T00:00:00Z',
    updatedutc              TEXT    NOT NULL DEFAULT '2026-01-01T00:00:00Z',
    PRIMARY KEY (bostromaddress, region),
    CHECK (intelligenceissovereign IN (0, 1)),
    CHECK (protectedstakeholder  IN (0, 1)),
    CHECK (bievidencemode IN ('HASHONLY','REDACTED')),
    CHECK (underattackstate IN ('NORMAL','UNDERATTACK','UNDERREVIEW','DISQUALIFIED'))
);

INSERT OR IGNORE INTO cyberaugcitizensovereign (
    bostromaddress,
    region,
    intelligenceissovereign,
    protectedstakeholder,
    karmafloor,
    bievidencemode,
    underattackstate,
    createdutc,
    updatedutc
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'Phoenix-AZ',
    1,
    1,
    0.80,
    'HASHONLY',
    'NORMAL',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);
