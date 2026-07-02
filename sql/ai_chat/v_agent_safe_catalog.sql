-- filename: sql/ai_chat/v_agent_safe_catalog.sql
-- destination: eco_restoration_shard/sql/ai_chat/v_agent_safe_catalog.sql
-- target-repo: github.com/mk-bluebird/eco_restoration_shard
--
-- Purpose:
-- AI-safe catalog view listing only non-actuating, governance-approved objects
-- (views, FFI exports, patterns) from eco_restoration_shard.
--
-- This becomes the authoritative list of AI-visible objects for tooling layers.
-- AI tools never need to guess what they may call - they consult this view.

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. AI-safe catalog view
-------------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_agent_safe_catalog AS
SELECT
    -- Normalized object identifier
    CASE
        WHEN d.kind = 'VIEW' THEN 'VIEW:' || d.logicalname
        WHEN d.kind = 'FFI' THEN 'FFI:' || d.logicalname
        WHEN d.kind = 'SQLPATTERN' THEN 'PATTERN:' || d.logicalname
        WHEN d.kind = 'PROMPT' THEN 'PROMPT:' || d.logicalname
        WHEN d.kind = 'ALN' THEN 'ALN:' || d.logicalname
        ELSE 'OTHER:' || d.logicalname
    END AS objectid,
    d.kind AS objectkind,
    d.repo AS reponame,
    r.roleband,
    r.lanedefault AS lanescope,
    -- AI capability level based on roleband and object kind
    CASE
        WHEN d.kind IN ('VIEW', 'SQLPATTERN') AND r.nonactuatingonly = 1 THEN 'READONLY_SPINE'
        WHEN d.kind = 'FFI' AND r.nonactuatingonly = 1 THEN 'READONLY_SPINE'
        WHEN d.kind = 'PROMPT' THEN 'DOC'
        WHEN d.kind = 'ALN' AND r.roleband IN ('SPINE', 'GOV') THEN 'PROTO_AGENT'
        ELSE 'NONE'
    END AS aicapabilitylevel,
    -- Path or handle for the object
    COALESCE(d.destinationpath, d.filename) AS path_or_handle,
    -- Contracts summary from repo layer
    COALESCE(
        (SELECT GROUP_CONCAT(DISTINCT l.contracts, '; ')
         FROM econetlayer l
         WHERE l.reponame = d.repo AND l.contracts IS NOT NULL AND l.contracts != ''),
        'NonActuatingWorkload'
    ) AS contractssummary,
    -- Human-readable summary from definitionregistry
    d.summary,
    -- Version tag
    d.versiontag,
    -- Status flag
    CASE
        WHEN d.active = 1 THEN 'ACTIVE'
        ELSE 'INACTIVE'
    END AS status,
    -- Risk band from repo
    CASE
        WHEN r.kertargetr <= 0.20 THEN 'LOW_RISK'
        WHEN r.kertargetr <= 0.40 THEN 'MODERATE_RISK'
        ELSE 'HIGH_RISK'
    END AS riskband,
    -- Non-actuating guarantee
    r.nonactuatingonly
FROM definitionregistry d
JOIN econetrepoindex r ON d.repo = r.reponame
WHERE
    -- Only allow SPINE, GOV, RESEARCH rolebands
    r.roleband IN ('SPINE', 'GOV', 'RESEARCH')
    -- Only non-actuating or diagnostic object kinds
    AND (
        r.nonactuatingonly = 1
        OR d.kind IN ('VIEW', 'FFI', 'SQLPATTERN', 'PROMPT', 'ALN')
    )
    -- Exclude FROZEN_DEPRECATED unless explicitly AI-approved
    AND NOT (
        d.versiontag LIKE '%FROZEN_DEPRECATED%'
        AND NOT EXISTS (
            SELECT 1 FROM econetrolehint h
            WHERE h.reponame = d.repo
            AND h.key = 'aiapproveddeprecated'
            AND h.value LIKE '%' || d.logicalname || '%'
        )
    )
ORDER BY objectkind, reponame, objectid;

-------------------------------------------------------------------------------
-- 2. Seed AI-visible objects into definitionregistry
-------------------------------------------------------------------------------

-- Register the AI manifest view
INSERT OR IGNORE INTO definitionregistry (
    contractid, scope, logicalname, kind, repo, destinationpath,
    filename, language, versiontag, active, primaryplane, appliescope,
    summary, signingdid, issuedutc, updatedutc
) VALUES (
    'EcosafetyContinuity2026v1', 'AICHAT', 'veconet_repo_manifest_agent', 'VIEW',
    'eco_restoration_shard', 'sql/ai_chat', 'veconet_repo_manifest_agent.sql',
    'SQL', '2026v1', 1, 'GOVERNANCE', 'AICHAT',
    'AI manifest view providing repo safety/lanes/contracts in one query.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now'), datetime('now')
);

-- Register the safe catalog view itself
INSERT OR IGNORE INTO definitionregistry (
    contractid, scope, logicalname, kind, repo, destinationpath,
    filename, language, versiontag, active, primaryplane, appliescope,
    summary, signingdid, issuedutc, updatedutc
) VALUES (
    'EcosafetyContinuity2026v1', 'AICHAT', 'v_agent_safe_catalog', 'VIEW',
    'eco_restoration_shard', 'sql/ai_chat', 'v_agent_safe_catalog.sql',
    'SQL', '2026v1', 1, 'GOVERNANCE', 'AICHAT',
    'Authoritative catalog of AI-visible non-actuating objects.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now'), datetime('now')
);

-- Register blast-radius view
INSERT OR IGNORE INTO definitionregistry (
    contractid, scope, logicalname, kind, repo, destinationpath,
    filename, language, versiontag, active, primaryplane, appliescope,
    summary, signingdid, issuedutc, updatedutc
) VALUES (
    'BlastRadius2026v1', 'AICHAT', 'vmachine_blastradius', 'VIEW',
    'eco_restoration_shard', 'sql/ai_chat', 'vmachine_blastradius.sql',
    'SQL', '2026v1', 1, 'RISK', 'AICHAT',
    'Blast-radius summary per node/machine across HYDRAULIC, CARBON, BIODIVERSITY planes.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now'), datetime('now')
);

-- Register workload window view
INSERT OR IGNORE INTO definitionregistry (
    contractid, scope, logicalname, kind, repo, destinationpath,
    filename, language, versiontag, active, primaryplane, appliescope,
    summary, signingdid, issuedutc, updatedutc
) VALUES (
    'EcosafetyContinuity2026v1', 'AICHAT', 'vcyboworkloadnodewindow', 'VIEW',
    'eco_restoration_shard', 'sql/ai_chat', 'vcyboworkloadnodewindow.sql',
    'SQL', '2026v1', 1, 'ENERGY', 'AICHAT',
    'Windowed aggregates of cyboworkloadledger per node: ereqj, esurplusj, vtbefore, vtafter.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now'), datetime('now')
);

-- Register FFI functions as AI-visible
INSERT OR IGNORE INTO definitionregistry (
    contractid, scope, logicalname, kind, repo, destinationpath,
    filename, language, versiontag, active, primaryplane, appliescope,
    summary, signingdid, issuedutc, updatedutc
) VALUES
('EcosafetyContinuity2026v1', 'AICHAT', 'econet_get_repo_manifest_agent', 'FFI',
 'eco_restoration_shard', 'src', 'lib.rs', 'Rust', '2026v1', 1, 'GOVERNANCE', 'AICHAT',
 'Returns JSON manifest for a repo: safety, lanes, contracts.',
 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7', datetime('now'), datetime('now')),
('EcosafetyContinuity2026v1', 'AICHAT', 'econet_get_agent_safe_catalog', 'FFI',
 'eco_restoration_shard', 'src', 'lib.rs', 'Rust', '2026v1', 1, 'GOVERNANCE', 'AICHAT',
 'Returns JSON array of AI-safe catalog objects filtered by repo.',
 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7', datetime('now'), datetime('now')),
('EcosafetyContinuity2026v1', 'AICHAT', 'econet_get_node_window', 'FFI',
 'eco_restoration_shard', 'src', 'lib.rs', 'Rust', '2026v1', 1, 'ENERGY', 'AICHAT',
 'Returns JSON window aggregates for a Cyboquatic node.',
 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7', datetime('now'), datetime('now')),
('EcosafetyContinuity2026v1', 'AICHAT', 'econet_get_blastradius_summary', 'FFI',
 'eco_restoration_shard', 'src', 'lib.rs', 'Rust', '2026v1', 1, 'RISK', 'AICHAT',
 'Returns JSON blast-radius summary for a node/machine.',
 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7', datetime('now'), datetime('now'));

-------------------------------------------------------------------------------
-- 3. Indexes for efficient catalog lookups
-------------------------------------------------------------------------------

CREATE INDEX IF NOT EXISTS idx_definitionregistry_kind_active
    ON definitionregistry (kind, active);

CREATE INDEX IF NOT EXISTS idx_definitionregistry_repo_kind
    ON definitionregistry (repo, kind);
