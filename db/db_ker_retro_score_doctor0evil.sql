-- filename db_ker_retro_score_doctor0evil.sql
-- destination Eco-Fort/db/db_ker_retro_score_doctor0evil.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 35. Retroactive KER-scoring for legacy Doctor0Evil contributions
-------------------------------------------------------------------------------

-- Assumptions:
--   repo(name, githubslug, repoid, roleband, ...)
--   shardinstance(shardid, repofileid, kmetric, emetric, rmetric, lane, kerdeployable, ...)
--   repofile(fileid, repoid, ...)
--   definition_registry(particlename, primaryplane, repotarget, ...)
--   knowledgeecoscore(scoreid, scopetype, scoperefid, kfactor, efactor, rfactor,
--                     rationale, timestamputc, issuedby)

-- 35.1 Helper view: legacy Doctor0Evil repos
CREATE VIEW IF NOT EXISTS v_doctor0evil_repo AS
SELECT
    repoid,
    name,
    githubslug,
    roleband
FROM repo
WHERE githubslug LIKE 'Doctor0Evil/%';

-- 35.2 Helper view: shard counts and averages per repo
CREATE VIEW IF NOT EXISTS v_repo_shard_aggregates AS
SELECT
    r.repoid,
    COUNT(si.shardid) AS shard_count,
    AVG(CASE WHEN si.kmetric IS NOT NULL THEN si.kmetric ELSE 0 END) AS avg_k,
    AVG(CASE WHEN si.emetric IS NOT NULL THEN si.emetric ELSE 0 END) AS avg_e,
    AVG(CASE WHEN si.rmetric IS NOT NULL THEN si.rmetric ELSE 0 END) AS avg_r,
    SUM(CASE WHEN si.kerdeployable = 1 THEN 1 ELSE 0 END) AS deployable_count,
    SUM(CASE WHEN si.lane = 'RESEARCH' THEN 1 ELSE 0 END) AS research_count,
    SUM(CASE WHEN si.lane IN ('EXPPROD','PROD') THEN 1 ELSE 0 END) AS actuating_count
FROM repo r
JOIN repofile rf ON rf.repoid = r.repoid
JOIN shardinstance si ON si.repofileid = rf.fileid
GROUP BY r.repoid;

-- 35.3 Helper view: plane-weighted eco-impact from definitions
CREATE VIEW IF NOT EXISTS v_repo_definition_impact AS
SELECT
    r.repoid,
    COUNT(DISTINCT dr.particlename) AS def_count,
    SUM(CASE dr.primaryplane
            WHEN 'biodiversity' THEN 1.0
            WHEN 'carbon' THEN 0.9
            WHEN 'hydrologyMAR' THEN 0.8
            ELSE 0.5
        END) AS plane_impact_score
FROM repo r
LEFT JOIN definition_registry dr
  ON dr.repotarget = r.name
GROUP BY r.repoid;

-- 35.4 Composite scoring for legacy Doctor0Evil repos
WITH legacy AS (
    SELECT
        d.repoid,
        d.name,
        d.githubslug,
        COALESCE(ra.shard_count, 0) AS shard_count,
        COALESCE(ra.avg_k, 0.0) AS avg_k,
        COALESCE(ra.avg_e, 0.0) AS avg_e,
        COALESCE(ra.avg_r, 0.0) AS avg_r,
        COALESCE(ra.research_count, 0) AS research_count,
        COALESCE(ra.actuating_count, 0) AS actuating_count,
        COALESCE(di.def_count, 0) AS def_count,
        COALESCE(di.plane_impact_score, 0.0) AS plane_impact_score
    FROM v_doctor0evil_repo d
    LEFT JOIN v_repo_shard_aggregates ra ON ra.repoid = d.repoid
    LEFT JOIN v_repo_definition_impact di ON di.repoid = d.repoid
), scored AS (
    SELECT
        repoid,
        name,
        githubslug,
        shard_count,
        def_count,
        plane_impact_score,
        avg_k,
        avg_e,
        avg_r,
        research_count,
        actuating_count,
        CASE
            WHEN shard_count = 0 THEN 0.70
            ELSE LEAST(1.0,
                0.60 + 0.20 * (plane_impact_score / (1.0 + def_count)) +
                0.20 * avg_k
            )
        END AS kfactor,
        CASE
            WHEN shard_count = 0 THEN 0.80
            ELSE LEAST(1.0,
                0.50 + 0.30 * (plane_impact_score / (1.0 + def_count)) +
                0.20 * avg_e
            )
        END AS efactor,
        CASE
            WHEN shard_count = 0 THEN 0.20
            ELSE GREATEST(
                0.05,
                LEAST(
                    1.0,
                    avg_r + 0.10 * (CAST(actuating_count AS REAL) / (1.0 + shard_count))
                )
            )
        END AS rfactor
    FROM legacy
)
INSERT INTO knowledgeecoscore (
    scopetype,
    scoperefid,
    kfactor,
    efactor,
    rfactor,
    rationale,
    timestamputc,
    issuedby
)
SELECT
    'REPO' AS scopetype,
    repoid AS scoperefid,
    kfactor,
    efactor,
    rfactor,
    'Retroactive KER score for legacy Doctor0Evil repo based on shard count, plane impact, and actuating share.' AS rationale,
    datetime('now') AS timestamputc,
    'did:bostrom:bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7' AS issuedby
FROM scored;
