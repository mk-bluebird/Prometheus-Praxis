-- filename dbequation_intelligence_is_sovereign_register_phoenix.sql
-- destination eco_restoration_shard/db/dbequation_intelligence_is_sovereign_register_phoenix.sql
-- repo-target github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-----------------------------------------------------------------------
-- Manifest-level equation view for Phoenix
-- Logicalname: equation.intelligence_is_sovereign.phoenix.2026v1
-----------------------------------------------------------------------

-------------------------------------------------------------------------------
-- 1. Register equation SQL and followup prompt in repofile + definitionregistry
-------------------------------------------------------------------------------

-- Equation SQL file registration (governance DB)
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
    'db/dbequation_intelligence_is_sovereign_register_phoenix.sql' AS relpath,
    'GOVERNANCEDB'                                                 AS purpose,
    'SQL'                                                          AS language,
    '2026-01-01T00:00:00Z'                                         AS createdutc,
    '2026-01-01T00:00:00Z'                                         AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- Equation SQL file registration (schema surface)
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
    'db/dbequation_intelligence_is_sovereign_register_phoenix.sql' AS relpath,
    'SQLSCHEMA'                                                    AS purpose,
    'SQL'                                                          AS language,
    '2026-01-01T00:00:00Z'                                         AS createdutc,
    '2026-01-01T00:00:00Z'                                         AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- Followup prompt Markdown registration
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
    'docs/prompts/followup_intelligence_is_sovereign_phx_2026v1.md' AS relpath,
    'DOCSPEC'                                                        AS purpose,
    'MARKDOWN'                                                       AS language,
    '2026-01-01T00:00:00Z'                                           AS createdutc,
    '2026-01-01T00:00:00Z'                                           AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- Definition registry entry for the Phoenix bioscale fairness equation
INSERT OR IGNORE INTO definitionregistryrestoration (
    defid,
    logicalname,
    repotarget,
    sqlfile,
    region,
    ecoscope,
    laneband,
    versiontag,
    createdutc,
    updatedutc
) VALUES (
    'equation.intelligence_is_sovereign.phoenix.2026v1',
    'equation.intelligence_is_sovereign.phoenix.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/dbequation_intelligence_is_sovereign_register_phoenix.sql',
    'Phoenix-AZ',
    'NEURORIGHTS,BIOSCALEFAIRNESS',
    'GOV',
    '2026v1',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);

-------------------------------------------------------------------------------
-- 2. Bind sovereign identity to equation and Phoenix sovereign config
-------------------------------------------------------------------------------

-- Identity binding for the INTELLIGENCE_IS_SOVEREIGN equation
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
    'db/dbequation_intelligence_is_sovereign_register_phoenix.sql',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCEDB',
    'IntelligenceIsSovereignPhoenix2026v1',
    'Bioscale fairness equation enforcing INTELLIGENCE_IS_SOVEREIGN invariants for augmented citizens in Phoenix.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);

-- Identity binding for the Phoenix sovereign config TOML
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
    'Sovereign configuration stating INTELLIGENCE_IS_SOVEREIGN and forbidding psych-risk weaponization against the bound brain-identity.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);

-------------------------------------------------------------------------------
-- 3. Agent followup prompt shard wiring
-------------------------------------------------------------------------------

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
    'followup.intelligence_is_sovereign.phoenix.2026v1'                           AS logicalname,
    '2026v1'                                                                      AS versiontag,
    'Phoenix-AZ'                                                                  AS region,
    'REGION'                                                                      AS scope,
    rf.fileid                                                                     AS repofileid,
    'AGENTPROMPT'                                                                 AS ecoscope,
    'GOV'                                                                         AS laneband,
    'AICHAT'                                                                      AS consumerkind,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'                              AS bostromaddress,
    'IntelligenceIsSovereignFollowupPhoenix2026v1'                                AS contractid,
    'Before approving any manifest, query vagent_intelligence_is_sovereign_phx. If bioscalefairnessdecision equals AUTODENY_INTELLIGENCE_IS_SOVEREIGN for the bound CyberneticallyAugmentedCitizen, reject the manifest and emit a governance notice stating that the brain-bound identity’s sovereignty cannot be degraded.' AS prompttext,
    'Use vagent_intelligence_is_sovereign_phx to enforce INTELLIGENCE_IS_SOVEREIGN invariants and auto-deny any manifest that attempts to discredit, exclude, or sabotage a protected brain-bound identity.' AS equationtext,
    'Neurorights and bioscale fairness followup that treats intelligence as sovereign and forbids downgrades of protected augmented citizens without proven harm and neurorights consent.' AS freedomstance,
    '2026-01-01T00:00:00Z'                                                       AS createdutc,
    '2026-01-01T00:00:00Z'                                                       AS updatedutc
FROM repofile AS rf
JOIN repo     AS r  ON r.repoid = rf.repoid
WHERE r.name  = 'eco_restoration_shard'
  AND rf.relpath = 'docs/prompts/followup_intelligence_is_sovereign_phx_2026v1.md';

-- Identity binding for the followup prompt object
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
    'followup.intelligence_is_sovereign.phoenix.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'docs/prompts/followup_intelligence_is_sovereign_phx_2026v1.md',
    'Phoenix-AZ',
    'REGION',
    'DOCSPEC',
    'IntelligenceIsSovereignFollowupPhoenix2026v1',
    'Followup object for bioscale fairness validation that auto-denies manifests violating the INTELLIGENCE_IS_SOVEREIGN neurorights clause.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);

-------------------------------------------------------------------------------
-- 4. Agent-facing view: vagent_intelligence_is_sovereign_phx
-------------------------------------------------------------------------------

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

-------------------------------------------------------------------------------
-- 5. DefinitionRegistry entry (normalized form) for equation surface
-------------------------------------------------------------------------------

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
    'equation.intelligence_is_sovereign.phoenix.2026v1'                       AS logicalname,
    '2026v1'                                                                  AS versiontag,
    lower(hex(randomblob(16)))                                               AS hash,
    'ACTIVE'                                                                  AS status,
    r.repoid                                                                  AS repoid,
    'db/dbequation_intelligence_is_sovereign_register_phoenix.sql'           AS relpathsql,
    NULL                                                                      AS relpathaln,
    NULL                                                                      AS relpathdoc,
    'NEURORIGHTS,BIOSCALEFAIRNESS'                                            AS ecoscope,
    'IntelligenceIsSovereignPhoenix2026v1'                                    AS contractid,
    '2026-01-01T00:00:00Z'                                                    AS createdutc,
    '2026-01-01T00:00:00Z'                                                    AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';
