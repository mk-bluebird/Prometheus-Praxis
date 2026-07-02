-- filename: sql/ai_chat/veconet_repo_manifest_agent.sql
-- destination: eco_restoration_shard/sql/ai_chat/veconet_repo_manifest_agent.sql
-- target-repo: github.com/mk-bluebird/eco_restoration_shard
--
-- Purpose:
-- Provide a single-view manifest for AI-chat agents answering:
-- "What is this repo, what lanes and contracts apply, and is actuation forbidden here?"
--
-- This view joins econetrepoindex with layer/role hints to expose:
-- reponame, githubslug, roleband, lanedefault, nonactuatingonly, region, didowner,
-- primarylanguages (aggregated), contractssummary (from policy shards).
--
-- Hard requirement: eco_restoration_shard marked as roleband='RESEARCH', nonactuatingonly=1.

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. AI manifest view over repo metadata
-------------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS veconet_repo_manifest_agent AS
SELECT
    r.reponame,
    r.githubslug,
    r.roleband,
    r.lanedefault,
    r.nonactuatingonly,
    r.region,
    r.didowner,
    -- Aggregate primary languages from econetlayer
    COALESCE(
        (SELECT GROUP_CONCAT(DISTINCT l.languages, ', ')
         FROM econetlayer l
         WHERE l.reponame = r.reponame AND l.active != 0),
        r.languageprimary
    ) AS primarylanguages,
    -- Contracts summary from attached policy shards / layers
    COALESCE(
        (SELECT GROUP_CONCAT(DISTINCT l.contracts, '; ')
         FROM econetlayer l
         WHERE l.reponame = r.reponame AND l.contracts IS NOT NULL AND l.contracts != ''),
        'NonActuatingWorkload; NoFFIActuators; NoCorridorWeakening'
    ) AS contractssummary,
    -- Repo-level KER targets
    r.kertargetk,
    r.kertargete,
    r.kertargetr,
    -- Computed risk band for AI (max of R-axis)
    CASE
        WHEN r.kertargetr <= 0.20 THEN 'LOW_RISK'
        WHEN r.kertargetr <= 0.40 THEN 'MODERATE_RISK'
        ELSE 'HIGH_RISK'
    END AS riskband,
    -- AI capability flags
    CASE
        WHEN r.nonactuatingonly = 1 AND r.roleband = 'RESEARCH' THEN 'MAY_ANSWER_KER_ONLY'
        WHEN r.nonactuatingonly = 1 AND r.roleband IN ('SPINE', 'GOV') THEN 'MAY_PROPOSE_NONACTUATING_SQL'
        ELSE 'HUMAN_REVIEW_REQUIRED'
    END AS aicapabilityflag,
    -- Evidence and signing
    r.signingdid,
    r.evidencehex,
    r.manifestschema_version
FROM econetrepoindex r
WHERE r.active != 0 OR r.active IS NULL;

-------------------------------------------------------------------------------
-- 2. Ensure eco_restoration_shard is marked RESEARCH / nonactuatingonly=1
-------------------------------------------------------------------------------

-- Upsert the eco_restoration_shard entry with correct AI-safe flags
INSERT OR REPLACE INTO econetrepoindex (
    reponame,
    githubslug,
    roleband,
    visibility,
    languageprimary,
    description,
    ecosafetybinding,
    shardprotocol,
    lanedefault,
    kertargetk,
    kertargete,
    kertargetr,
    nonactuatingonly,
    didowner,
    signingdid,
    evidencehex,
    manifestschema_version
) VALUES (
    'eco_restoration_shard',
    'mk-bluebird/eco_restoration_shard',
    'RESEARCH',
    'Public',
    'Rust',
    'Non-actuating restoration shard for Cyboquatic diagnostics, blast-radius, and KER analysis. AI-chat safe.',
    'cyboquatic-ecosafety-core2026v1.aln',
    'EcoNetSchemaShard2026v1',
    'RESEARCH',
    0.95,
    0.91,
    0.12,
    1,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    '0xAIREPOSPECHEX2026MANIFEST',
    1
);

-- Layer entries for eco_restoration_shard
INSERT OR IGNORE INTO econetlayer (
    reponame,
    layername,
    layertier,
    languages,
    description,
    contracts,
    active
) VALUES
('eco_restoration_shard', 'SpineIndex', 'INDEX', 'Rust,SQL',
 'Core spine index for blast-radius, workload ledgers, and Cyboquatic surfaces.',
 'NonActuatingWorkload; NoFFIActuators; NoCorridorWeakening', 1),
('eco_restoration_shard', 'AIDiagnostics', 'RESEARCH', 'Rust,Lua,Kotlin',
 'AI-visible diagnostic exports: manifest, safe catalog, node windows, blast-radius summaries.',
 'NonActuatingWorkload; AIReadOnlyAccess', 1),
('eco_restoration_shard', 'PolicyBindings', 'GOV', 'ALN,SQL',
 'Governance policy bindings for KER ceilings, lane admissibility, and RoH constraints.',
 'KERCeilingEnforced; LaneAdmissibilityChecked', 1);

-- Role hints for AI interpretation
INSERT OR IGNORE INTO econetrolehint (reponame, key, value) VALUES
('eco_restoration_shard', 'aivisibleviews', 'veconet_repo_manifest_agent;v_agent_safe_catalog;vmachine_blastradius;vcyboworkloadnodewindow'),
('eco_restoration_shard', 'aipatterns', 'PATTERN_SAFE_PROD_SHARDS;PATTERN_CYBO_NODE_IMPROVEMENT'),
('eco_restoration_shard', 'riskgrammar', 'rtopology;rbio;rthermal;rcarbon;rgovernance'),
('eco_restoration_shard', 'nonoffsettableplanes', 'carbon;biodiversity;hydraulic');

-------------------------------------------------------------------------------
-- 3. Indexes for efficient AI manifest lookups
-------------------------------------------------------------------------------

CREATE INDEX IF NOT EXISTS idx_econetrepo_roleband_nonactuating
    ON econetrepoindex (roleband, nonactuatingonly);

CREATE INDEX IF NOT EXISTS idx_econetlayer_reponame_active
    ON econetlayer (reponame, active);
