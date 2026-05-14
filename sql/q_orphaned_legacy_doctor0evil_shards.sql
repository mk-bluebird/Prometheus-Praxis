-- filename q_orphaned_legacy_doctor0evil_shards.sql
-- destination eco_restoration_shard/sql/q_orphaned_legacy_doctor0evil_shards.sql

SELECT
    s.shardid,
    s.nodeid,
    s.region,
    s.lane,
    s.kmetric,
    s.emetric,
    s.rmetric,
    s.tstartutc,
    s.tendutc,
    ae.legacy_login,
    ae.repo_id            AS legacy_repo_id
FROM shardinstance s
JOIN eco_author_evidence ae
    ON ae.shardid = s.shardid
WHERE ae.legacy_login LIKE 'Doctor0Evil%'
  AND NOT EXISTS (
      SELECT 1
      FROM eco_repo_identity_binding b
      JOIN repo r
        ON r.repoid = b.repo_id
      WHERE b.is_current_owner = 1
        AND b.active = 1
        AND r.githubslug LIKE 'mk-bluebird/%'
        AND r.repoid = ae.repo_id
  );
