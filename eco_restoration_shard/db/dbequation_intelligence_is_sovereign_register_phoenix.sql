-- filename dbequation_intelligence_is_sovereign_register_phoenix.sql
-- destination eco_restoration_shard/db/dbequation_intelligence_is_sovereign_register_phoenix.sql
-- repo-target github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-----------------------------------------------------------------------
-- Manifest-level equation view for Phoenix
-- Logicalname: equation.intelligence_is_sovereign.phoenix.2026v1
-----------------------------------------------------------------------

DROP VIEW IF EXISTS vagent_intelligence_is_sovereign_phx;

CREATE VIEW vagent_intelligence_is_sovereign_phx AS
SELECT
    m.manifestid,
    m.logicalname      AS manifestlogicalname,
    m.region,
    m.ecoscope,
    m.laneband,
    m.status           AS manifeststatus,
    rib.bostromaddress,
    cis.intelligenceissovereign,
    cis.protectedstakeholder,
    cis.karmafloor,
    cis.bievidencemode,
    cis.underattackstate,
    cis.bimodeviolation,
    cis.karmafloorviolation,
    cis.underattackfreeze,
    CASE
        WHEN cis.intelligenceissovereign = 1
         AND (
              cis.bimodeviolation      = 1
           OR cis.karmafloorviolation = 1
           OR cis.underattackfreeze   = 1
         )
        THEN 1
        ELSE 0
    END AS intelligence_is_sovereign_violation,
    CASE
        WHEN cis.intelligenceissovereign = 1
         AND (
              cis.bimodeviolation      = 1
           OR cis.karmafloorviolation = 1
           OR cis.underattackfreeze   = 1
         )
        THEN 'AUTODENY_INTELLIGENCE_IS_SOVEREIGN'
        ELSE 'OK'
    END AS bioscalefairnessdecision
FROM manifestindex AS m
JOIN restorationidentitybinding AS rib
  ON rib.logicalname = m.logicalname
 AND rib.region      = m.region
JOIN vbioscale_intelligence_is_sovereign AS cis
  ON cis.bostromaddress = rib.bostromaddress
WHERE m.region = 'Phoenix-AZ';

-----------------------------------------------------------------------
-- 2. Register the equation view in the DefinitionRegistry and bindings
-----------------------------------------------------------------------

-- Register the equation SQL in repofile.
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
    'db/dbequation_intelligence_is_sovereign_phoenix.sql' AS relpath,
    'SQLSCHEMA' AS purpose,
    'SQL' AS language,
    '2026-01-01T00:00:00Z' AS createdutc,
    '2026-01-01T00:00:00Z' AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- DefinitionRegistry row for the Phoenix sovereignty bioscale equation.
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
    'db/dbequation_intelligence_is_sovereign_phoenix.sql' AS relpathsql,
    NULL AS relpathaln,
    NULL AS relpathdoc,
    'NEURORIGHTS' AS ecoscope,
    'IntelligenceIsSovereignPhoenix2026v1' AS contractid,
    '2026-01-01T00:00:00Z' AS createdutc,
    '2026-01-01T00:00:00Z' AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- Bind the equation definition to the Phoenix sovereign identity.
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
    'equation.intelligence_is_sovereign.phoenix.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/dbequation_intelligence_is_sovereign_phoenix.sql',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCEDB',
    'IntelligenceIsSovereignPhoenix2026v1',
    'Bioscale fairness equation enforcing the INTELLIGENCE_IS_SOVEREIGN clause for CyberneticallyAugmentedCitizen identities in Phoenix.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);
