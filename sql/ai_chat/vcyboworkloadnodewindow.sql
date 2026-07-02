-- filename: sql/ai_chat/vcyboworkloadnodewindow.sql
-- destination: eco_restoration_shard/sql/ai_chat/vcyboworkloadnodewindow.sql
-- target-repo: github.com/mk-bluebird/eco_restoration_shard
--
-- Purpose:
-- Narrowed view for AI-chat summarizing cyboworkloadledger per node,
-- with windowed aggregates: total ereqj, esurplusj, mean vtbefore, vtafter, accept fractions.

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Cyboquatic workload node window view
-------------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vcyboworkloadnodewindow AS
SELECT
    nodeid,
    channel,
    MIN(timestamputc) AS window_start_utc,
    MAX(timestamputc) AS window_end_utc,
    COUNT(*) AS workload_count,
    -- Energy aggregates
    SUM(ereqj) AS total_ereqj,
    SUM(esurplusj) AS total_esurplusj,
    -- Surplus fraction (efficiency metric)
    CASE
        WHEN SUM(ereqj) > 0 THEN SUM(esurplusj) / SUM(ereqj)
        ELSE 0.0
    END AS surplus_fraction,
    -- Accept/reject/reroute counts
    SUM(CASE WHEN decision = 'ACCEPT' THEN 1 ELSE 0 END) AS accept_count,
    SUM(CASE WHEN decision = 'REJECT' THEN 1 ELSE 0 END) AS reject_count,
    SUM(CASE WHEN decision = 'REROUTE' THEN 1 ELSE 0 END) AS reroute_count,
    -- Accept fraction
    CASE
        WHEN COUNT(*) > 0 THEN
            CAST(SUM(CASE WHEN decision = 'ACCEPT' THEN 1 ELSE 0 END) AS REAL) / COUNT(*)
        ELSE 0.0
    END AS accept_fraction,
    -- Lyapunov residual aggregates
    AVG(vtbefore) AS mean_vtbefore,
    AVG(vtafter) AS mean_vtafter,
    AVG(vtafter - vtbefore) AS mean_delta_vt,
    -- Vt improvement flag
    CASE
        WHEN AVG(vtafter) < AVG(vtbefore) THEN 1
        ELSE 0
    END AS vt_improved,
    -- Risk aggregates
    AVG(COALESCE(rcarbon, 0.0)) AS mean_rcarbon,
    AVG(COALESCE(rbiodiv, 0.0)) AS mean_rbiodiv,
    -- Carbon safety flag
    CASE
        WHEN AVG(COALESCE(rcarbon, 0.0)) <= 0.13 THEN 1
        ELSE 0
    END AS carbon_safe,
    -- Biodiversity safety flag
    CASE
        WHEN AVG(COALESCE(rbiodiv, 0.0)) <= 0.15 THEN 1
        ELSE 0
    END AS biodiv_safe
FROM cyboworkloadledger
GROUP BY nodeid, channel
ORDER BY nodeid, channel;

-------------------------------------------------------------------------------
-- 2. Register in definitionregistry
-------------------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry (
    contractid, scope, logicalname, kind, repo, destinationpath,
    filename, language, versiontag, active, primaryplane, appliescope,
    summary, signingdid, issuedutc, updatedutc
) VALUES (
    'EcosafetyContinuity2026v1', 'AICHAT', 'vcyboworkloadnodewindow', 'VIEW',
    'eco_restoration_shard', 'sql/ai_chat', 'vcyboworkloadnodewindow.sql',
    'SQL', '2026v1', 1, 'ENERGY', 'AICHAT',
    'AI-safe workload window aggregates per node with vt and risk metrics.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now'), datetime('now')
);
