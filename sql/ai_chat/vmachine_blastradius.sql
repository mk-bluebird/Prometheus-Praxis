-- filename: sql/ai_chat/vmachine_blastradius.sql
-- destination: eco_restoration_shard/sql/ai_chat/vmachine_blastradius.sql
-- target-repo: github.com/mk-bluebird/eco_restoration_shard
--
-- Purpose:
-- Narrowed view for AI-chat summarizing blastradiuslink per node/machine
-- across planes (HYDRAULIC, CARBON, BIODIVERSITY) with precomputed vt sensitivity bands.

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Machine/node blast-radius summary view
-------------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vmachine_blastradius AS
SELECT
    b.sourceid AS nodeid,
    b.targettype,
    b.targetid,
    b.impacttype,
    SUM(b.impactscore) AS total_impact_score,
    AVG(COALESCE(b.vtsensitivity, 0.0)) AS mean_vt_sensitivity,
    MAX(COALESCE(b.vtsensitivity, 0.0)) AS max_vt_sensitivity,
    COUNT(*) AS link_count,
    -- Precomputed normalized vt sensitivity band
    CASE
        WHEN AVG(COALESCE(b.vtsensitivity, 0.0)) <= 0.05 THEN 'VT_LOW_SENSITIVITY'
        WHEN AVG(COALESCE(b.vtsensitivity, 0.0)) <= 0.10 THEN 'VT_MODERATE_SENSITIVITY'
        ELSE 'VT_HIGH_SENSITIVITY'
    END AS vt_sensitivity_band,
    -- Impact type classification
    CASE
        WHEN b.impacttype = 'HYDRAULICS' THEN 'WATER_PLANE'
        WHEN b.impacttype = 'CARBON' THEN 'CARBON_PLANE'
        WHEN b.impacttype = 'BIODIVERSITY' THEN 'BIO_PLANE'
        WHEN b.impacttype = 'ENERGY' THEN 'ENERGY_PLANE'
        WHEN b.impacttype = 'MATERIALS' THEN 'MATERIAL_PLANE'
        WHEN b.impacttype = 'GOVERNANCE' THEN 'GOV_PLANE'
        ELSE 'OTHER_PLANE'
    END AS plane_classification,
    -- Safety step flag (Lyapunov descent indicator)
    CASE
        WHEN AVG(COALESCE(b.vtsensitivity, 0.0)) <= 0.08 THEN 1
        ELSE 0
    END AS safestepok
FROM blastradiuslink b
WHERE b.sourcetype IN ('NODE', 'SHARD')
GROUP BY b.sourceid, b.targettype, b.targetid, b.impacttype
ORDER BY b.sourceid, b.impacttype, total_impact_score DESC;

-------------------------------------------------------------------------------
-- 2. Register in definitionregistry
-------------------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry (
    contractid, scope, logicalname, kind, repo, destinationpath,
    filename, language, versiontag, active, primaryplane, appliescope,
    summary, signingdid, issuedutc, updatedutc
) VALUES (
    'BlastRadius2026v1', 'AICHAT', 'vmachine_blastradius', 'VIEW',
    'eco_restoration_shard', 'sql/ai_chat', 'vmachine_blastradius.sql',
    'SQL', '2026v1', 1, 'RISK', 'AICHAT',
    'AI-safe blast-radius summary per node/machine with vt sensitivity bands.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now'), datetime('now')
);
