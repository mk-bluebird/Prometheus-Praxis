-- filename: sql/ai_chat/agentsqlpattern.sql
-- destination: eco_restoration_shard/sql/ai_chat/agentsqlpattern.sql
-- target-repo: github.com/mk-bluebird/eco_restoration_shard
--
-- Purpose:
-- Agent SQL pattern index table for approved, parameterized SQL templates.
-- AI agents fetch safe query patterns instead of synthesizing joins across
-- governance and Cyboquatic tables.

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Agent SQL pattern table
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS agentsqlpattern (
    patternid           TEXT PRIMARY KEY,  -- e.g. PATTERN_SAFE_PROD_SHARDS
    description         TEXT NOT NULL,     -- human-readable target
    sqltext             TEXT NOT NULL,     -- parameterized SQL using :region, :lane, etc.
    lanescope           TEXT NOT NULL,     -- allowed lanes: RESEARCH, EXPPROD, PROD (comma-separated)
    riskceilingnote     TEXT NOT NULL,     -- text referencing KER/RoH ceilings
    aicapabilitylevel   TEXT NOT NULL CHECK (
        aicapabilitylevel IN ('NONE', 'DOC', 'READONLY_SPINE', 'PROTO_AGENT')
    ),
    versiontag          TEXT NOT NULL DEFAULT '2026v1',
    status              TEXT NOT NULL CHECK (status IN ('ACTIVE', 'DEPRECATED')),
    createdutc          TEXT NOT NULL DEFAULT (datetime('now')),
    updatedutc          TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_agentsqlpattern_status
    ON agentsqlpattern (status);

CREATE INDEX IF NOT EXISTS idx_agentsqlpattern_lanescope
    ON agentsqlpattern (lanescope);

CREATE INDEX IF NOT EXISTS idx_agentsqlpattern_aicapability
    ON agentsqlpattern (aicapabilitylevel);

-------------------------------------------------------------------------------
-- 2. Seed initial patterns
-------------------------------------------------------------------------------

-- Pattern: Find PROD-eligible shards with high K, high E, low R in region X
INSERT OR REPLACE INTO agentsqlpattern (
    patternid, description, sqltext, lanescope, riskceilingnote,
    aicapabilitylevel, versiontag, status
) VALUES (
    'PATTERN_SAFE_PROD_SHARDS',
    'Find PROD-eligible shards with high K, high E, low R in a given region.',
    'SELECT shardid, nodeid, region, lane, kmetric, emetric, rmetric, vtmax
     FROM shardinstance
     WHERE region = :region
       AND lane IN (''RESEARCH'', ''EXPPROD'', ''PROD'')
       AND kerdeployable = 1
       AND kmetric >= :min_k
       AND emetric >= :min_e
       AND rmetric <= :max_r
     ORDER BY kmetric DESC, emetric DESC, rmetric ASC;',
    'RESEARCH,EXPPROD,PROD',
    'KER ceiling: K>=0.90, E>=0.85, R<=0.20 per RoHKernelMT6883_2026v1.aln',
    'READONLY_SPINE',
    '2026v1',
    'ACTIVE'
);

-- Pattern: List Cyboquatic nodes that improved vt and reduced carbon in the last window
INSERT OR REPLACE INTO agentsqlpattern (
    patternid, description, sqltext, lanescope, riskceilingnote,
    aicapabilitylevel, versiontag, status
) VALUES (
    'PATTERN_CYBO_NODE_IMPROVEMENT',
    'List Cyboquatic nodes that improved Lyapunov residual (vt) and reduced carbon risk in the last window.',
    'SELECT nodeid, channel,
            SUM(ereqj) AS total_requests_j,
            SUM(esurplusj) AS total_surplus_j,
            AVG(vtbefore) AS mean_vt_before,
            AVG(vtafter) AS mean_vt_after,
            AVG(vtafter - vtbefore) AS mean_delta_vt,
            AVG(rcarbon) AS mean_rcarbon
     FROM cyboworkloadledger
     WHERE timestamputc >= :window_start
       AND timestamputc <= :window_end
       AND decision = ''ACCEPT''
     GROUP BY nodeid, channel
     HAVING AVG(vtafter) < AVG(vtbefore)
        AND AVG(rcarbon) <= 0.13
     ORDER BY mean_delta_vt ASC;',
    'RESEARCH,EXPPROD',
    'Carbon risk ceiling: rcarbon <= 0.13 per EcoPerJoulePolicy2026v1.aln; Vt descent required.',
    'READONLY_SPINE',
    '2026v1',
    'ACTIVE'
);

-- Pattern: Get blast-radius summary for a node across all impact types
INSERT OR REPLACE INTO agentsqlpattern (
    patternid, description, sqltext, lanescope, riskceilingnote,
    aicapabilitylevel, versiontag, status
) VALUES (
    'PATTERN_NODE_BLASTRADIUS_SUMMARY',
    'Get blast-radius summary for a specific node across HYDRAULIC, CARBON, BIODIVERSITY planes.',
    'SELECT sourceid AS nodeid, impacttype,
            SUM(impactscore) AS impactscoresum,
            AVG(COALESCE(vtsensitivity, 0.0)) AS vtsensitivity_mean,
            COUNT(*) AS linkcount
     FROM blastradiuslink
     WHERE sourcetype = ''NODE'' AND sourceid = :nodeid
     GROUP BY sourceid, impacttype
     ORDER BY impactscoresum DESC;',
    'RESEARCH,EXPPROD,PROD',
    'Impact scores normalized 0..1; vt sensitivity from Lyapunov corridor analysis.',
    'READONLY_SPINE',
    '2026v1',
    'ACTIVE'
);

-- Pattern: Get repo manifest with AI capability flags
INSERT OR REPLACE INTO agentsqlpattern (
    patternid, description, sqltext, lanescope, riskceilingnote,
    aicapabilitylevel, versiontag, status
) VALUES (
    'PATTERN_REPO_MANIFEST_QUERY',
    'Query repo manifest for AI capability flags, roleband, and non-actuating status.',
    'SELECT reponame, githubslug, roleband, lanedefault, nonactuatingonly,
            didowner, kertargetk, kertargete, kertargetr,
            CASE WHEN kertargetr <= 0.20 THEN ''LOW_RISK'' ELSE ''MODERATE_RISK'' END AS riskband,
            CASE
                WHEN nonactuatingonly = 1 AND roleband = ''RESEARCH'' THEN ''MAY_ANSWER_KER_ONLY''
                WHEN nonactuatingonly = 1 AND roleband IN (''SPINE'', ''GOV'') THEN ''MAY_PROPOSE_NONACTUATING_SQL''
                ELSE ''HUMAN_REVIEW_REQUIRED''
            END AS aicapabilityflag
     FROM econetrepoindex
     WHERE reponame = :reponame;',
    'RESEARCH,EXPPROD,PROD',
    'AI capability determined by roleband and nonactuatingonly flag per EcoRepoIndex2026v1.aln.',
    'READONLY_SPINE',
    '2026v1',
    'ACTIVE'
);

-- Pattern: Get workload window aggregates for a node
INSERT OR REPLACE INTO agentsqlpattern (
    patternid, description, sqltext, lanescope, riskceilingnote,
    aicapabilitylevel, versiontag, status
) VALUES (
    'PATTERN_NODE_WORKLOAD_WINDOW',
    'Get windowed workload aggregates (energy, surplus, vt delta) for a Cyboquatic node.',
    'SELECT nodeid, channel,
            MIN(timestamputc) AS window_start_utc,
            MAX(timestamputc) AS window_end_utc,
            SUM(ereqj) AS total_requests_j,
            SUM(esurplusj) AS total_surplus_j,
            AVG(vtbefore) AS mean_vt_before,
            AVG(vtafter) AS mean_vt_after,
            AVG(vtafter - vtbefore) AS mean_delta_vt
     FROM cyboworkloadledger
     WHERE nodeid = :nodeid
     GROUP BY nodeid, channel
     ORDER BY channel;',
    'RESEARCH,EXPPROD',
    'Diagnostic-only view; no actuation. Vt descent indicates ecological improvement.',
    'READONLY_SPINE',
    '2026v1',
    'ACTIVE'
);

-- Pattern: Get AI-safe catalog entries for a repo
INSERT OR REPLACE INTO agentsqlpattern (
    patternid, description, sqltext, lanescope, riskceilingnote,
    aicapabilitylevel, versiontag, status
) VALUES (
    'PATTERN_AI_SAFE_CATALOG_LOOKUP',
    'List all AI-visible objects (views, FFI, patterns) for a given repo.',
    'SELECT objectid, objectkind, reponame, roleband, lanescope,
            aicapabilitylevel, path_or_handle, contractssummary, status
     FROM v_agent_safe_catalog
     WHERE reponame = :reponame
     ORDER BY objectkind, objectid;',
    'RESEARCH,EXPPROD,PROD',
    'Catalog filtered to non-actuating objects only per v_agent_safe_catalog definition.',
    'READONLY_SPINE',
    '2026v1',
    'ACTIVE'
);

-------------------------------------------------------------------------------
-- 3. Register patterns in definitionregistry
-------------------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry (
    contractid, scope, logicalname, kind, repo, destinationpath,
    filename, language, versiontag, active, primaryplane, appliescope,
    summary, signingdid, issuedutc, updatedutc
) VALUES
('EcosafetyContinuity2026v1', 'AICHAT', 'PATTERN_SAFE_PROD_SHARDS', 'SQLPATTERN',
 'eco_restoration_shard', 'sql/ai_chat', 'agentsqlpattern.sql', 'SQL', '2026v1', 1,
 'GOVERNANCE', 'AICHAT', 'Safe pattern for PROD-eligible shard queries.',
 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7', datetime('now'), datetime('now')),
('EcosafetyContinuity2026v1', 'AICHAT', 'PATTERN_CYBO_NODE_IMPROVEMENT', 'SQLPATTERN',
 'eco_restoration_shard', 'sql/ai_chat', 'agentsqlpattern.sql', 'SQL', '2026v1', 1,
 'ENERGY', 'AICHAT', 'Pattern for finding Cyboquatic nodes with Vt improvement.',
 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7', datetime('now'), datetime('now')),
('BlastRadius2026v1', 'AICHAT', 'PATTERN_NODE_BLASTRADIUS_SUMMARY', 'SQLPATTERN',
 'eco_restoration_shard', 'sql/ai_chat', 'agentsqlpattern.sql', 'SQL', '2026v1', 1,
 'RISK', 'AICHAT', 'Pattern for blast-radius summary per node.',
 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7', datetime('now'), datetime('now')),
('EcosafetyContinuity2026v1', 'AICHAT', 'PATTERN_REPO_MANIFEST_QUERY', 'SQLPATTERN',
 'eco_restoration_shard', 'sql/ai_chat', 'agentsqlpattern.sql', 'SQL', '2026v1', 1,
 'GOVERNANCE', 'AICHAT', 'Pattern for repo manifest with AI capability flags.',
 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7', datetime('now'), datetime('now')),
('EcosafetyContinuity2026v1', 'AICHAT', 'PATTERN_NODE_WORKLOAD_WINDOW', 'SQLPATTERN',
 'eco_restoration_shard', 'sql/ai_chat', 'agentsqlpattern.sql', 'SQL', '2026v1', 1,
 'ENERGY', 'AICHAT', 'Pattern for node workload window aggregates.',
 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7', datetime('now'), datetime('now')),
('EcosafetyContinuity2026v1', 'AICHAT', 'PATTERN_AI_SAFE_CATALOG_LOOKUP', 'SQLPATTERN',
 'eco_restoration_shard', 'sql/ai_chat', 'agentsqlpattern.sql', 'SQL', '2026v1', 1,
 'GOVERNANCE', 'AICHAT', 'Pattern for AI-safe catalog lookup.',
 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7', datetime('now'), datetime('now'));
