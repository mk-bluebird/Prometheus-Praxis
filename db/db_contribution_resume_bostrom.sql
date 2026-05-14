-- filename db_contribution_resume_bostrom.sql
-- destination Eco-Fort/db/db_contribution_resume_bostrom.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 39. Contribution resume for a primary Bostrom address
-------------------------------------------------------------------------------
-- Assumptions:
--   shardinstance(shardid, repofileid, kmetric, emetric, rmetric, ...)
--   repofile(fileid, repoid, ...)
--   repo(repoid, name, ...)
--   eco_author_evidence(evidence_id, shardid, signingdid, ...)

-- Parameter: :primary_bostrom_did (e.g., 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7')

WITH authored AS (
    SELECT
        si.shardid,
        si.kmetric,
        si.emetric,
        si.rmetric,
        rf.repoid
    FROM eco_author_evidence ae
    JOIN shardinstance si ON si.shardid = ae.shardid
    JOIN repofile rf ON rf.fileid = si.repofileid
    WHERE ae.signingdid = :primary_bostrom_did
)
SELECT
    r.repoid,
    r.name          AS reponame,
    COUNT(a.shardid) AS shard_count,
    AVG(a.kmetric)   AS k_avg,
    AVG(a.emetric)   AS e_avg,
    AVG(a.rmetric)   AS r_avg
FROM authored a
JOIN repo r ON r.repoid = a.repoid
GROUP BY r.repoid, r.name
ORDER BY r.name;
