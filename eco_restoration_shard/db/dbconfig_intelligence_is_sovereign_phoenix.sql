-- filename dbconfig_intelligence_is_sovereign_phoenix.sql
-- destination eco_restoration_shard/db/dbconfig_intelligence_is_sovereign_phoenix.sql
-- repo-target github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-----------------------------------------------------------------------
-- 1. Sovereign configuration: Phoenix region, primary Bostrom identity
--    Mirrors config.sovereign.phoenix.2026v1.toml into SQL indices.
-----------------------------------------------------------------------

-- Register the TOML config file in repofile.
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

-- Register the sovereign config logical definition in the DefinitionRegistry.
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
    lower(hex(randomblob(16))) AS hash,
    'ACTIVE' AS status,
    r.repoid AS repoid,
    NULL AS relpathsql,
    NULL AS relpathaln,
    'config/config.sovereign.phoenix.2026v1.toml' AS relpathdoc,
    'AGENTPROMPT' AS ecoscope,
    'SovereignConfigPhoenix2026v1' AS contractid,
    '2026-01-01T00:00:00Z' AS createdutc,
    '2026-01-01T00:00:00Z' AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- Bind the sovereign config to the primary Phoenix Bostrom address.
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

-----------------------------------------------------------------------
-- 2. CyberneticallyAugmentedCitizen sovereign state slice
--    Per-identity clause flags for INTELLIGENCE_IS_SOVEREIGN in Phoenix.
-----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cyberaugcitizensovereign (
    bostromaddress        TEXT PRIMARY KEY,
    region                TEXT NOT NULL,
    intelligenceissovereign INTEGER NOT NULL DEFAULT 0,   -- 1 = clause active
    protectedstakeholder  INTEGER NOT NULL DEFAULT 0,     -- 1 = neurorights floor
    karmafloor            REAL    NOT NULL DEFAULT 0.0,   -- minimum K without proven harm
    bievidencemode        TEXT    NOT NULL DEFAULT 'HASHONLY', -- 'HASHONLY' or 'REDACTED'
    underattackstate      TEXT    NOT NULL DEFAULT 'NORMAL',   -- NORMAL, UNDERATTACK, UNDERREVIEW, DISQUALIFIED
    createdutc            TEXT    NOT NULL DEFAULT '2026-01-01T00:00:00Z',
    updatedutc            TEXT    NOT NULL DEFAULT '2026-01-01T00:00:00Z',
    CHECK (intelligenceissovereign IN (0,1)),
    CHECK (protectedstakeholder IN (0,1)),
    CHECK (bievidencemode IN ('HASHONLY','REDACTED')),
    CHECK (underattackstate IN ('NORMAL','UNDERATTACK','UNDERREVIEW','DISQUALIFIED'))
);

-- Seed sovereign clause state for the Phoenix primary identity.
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
