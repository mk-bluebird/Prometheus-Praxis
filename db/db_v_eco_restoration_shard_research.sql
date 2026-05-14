-- filename db_v_eco_restoration_shard_research.sql
-- destination eco_restoration_shard/db/db_v_eco_restoration_shard_research.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 36. v_eco_restoration_shard_research bound via author evidence and repo binding
-------------------------------------------------------------------------------
-- Assumptions:
--   shardinstance(shardid, repofileid, kmetric, emetric, rmetric, lane, ...)
--   repofile(fileid, repoid, ...)
--   repo(repoid, name, githubslug, ...)
--   eco_author_evidence(evidence_id, shardid, signingdid, ...)
--   eco_repo_identity_binding(
--       binding_id INTEGER PRIMARY KEY,
--       repoid     INTEGER NOT NULL REFERENCES repo(repoid),
--       signingdid TEXT    NOT NULL,
--       identity_kind TEXT NOT NULL, -- e.g. 'BOSTROM_DID'
--       active     INTEGER NOT NULL CHECK (active IN (0,1))
--   )

CREATE VIEW IF NOT EXISTS v_eco_restoration_shard_research AS
SELECT
    si.shardid,
    si.nodeid,
    si.region,
    si.lane,
    si.kmetric,
    si.emetric,
    si.rmetric,
    si.vtmax,
    si.kerdeployable,
    rf.fileid      AS repofileid,
    rf.relpath,
    rf.filename,
    r.repoid,
    r.name         AS reponame,
    r.githubslug,
    ae.signingdid,
    aeb.identity_kind,
    aeb.identity_binding_id
FROM shardinstance          AS si
JOIN repofile               AS rf  ON rf.fileid = si.repofileid
JOIN repo                   AS r   ON r.repoid = rf.repoid
JOIN eco_author_evidence    AS ae  ON ae.shardid = si.shardid
JOIN eco_repo_identity_binding AS aeb
  ON aeb.repoid = r.repoid
 AND aeb.signingdid = ae.signingdid
 AND aeb.active = 1
WHERE r.name = 'eco_restoration_shard';
