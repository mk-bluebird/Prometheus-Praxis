-- filename db_aln_migration_doctor0evil_to_mk_bluebird.sql
-- destination eco_restoration_shard/db/db_aln_migration_doctor0evil_to_mk_bluebird.sql

PRAGMA foreign_keys = OFF;

BEGIN IMMEDIATE TRANSACTION;

-------------------------------------------------------------------------------
-- 1. Identify source (Doctor0Evil) and target (mk-bluebird) repo ids
-------------------------------------------------------------------------------

WITH
    src_repo AS (
        SELECT repoid
        FROM repo
        WHERE githubslug = 'Doctor0Evil/eco_restoration_shard'
        LIMIT 1
    ),
    dst_repo AS (
        SELECT repoid
        FROM repo
        WHERE githubslug = 'mk-bluebird/eco_restoration_shard'
        LIMIT 1
    )

-------------------------------------------------------------------------------
-- 2. Remap alnschema rows from src_repo to dst_repo
-------------------------------------------------------------------------------

UPDATE alnschema
SET repofileid = (
        SELECT rf_dst.fileid
        FROM src_repo
        JOIN dst_repo
        JOIN repofile rf_src
          ON rf_src.fileid = alnschema.repofileid
        JOIN repofile rf_dst
          ON rf_dst.repoid = (SELECT repoid FROM dst_repo)
         AND rf_dst.relpath = rf_src.relpath
         AND rf_dst.filename = rf_src.filename
     )
WHERE repofileid IN (
    SELECT rf_src.fileid
    FROM src_repo
    JOIN repofile rf_src
      ON rf_src.repoid = (SELECT repoid FROM src_repo)
);

-------------------------------------------------------------------------------
-- 3. Remap shardinstance particle bindings (alnparticle) if needed
-------------------------------------------------------------------------------

UPDATE shardinstance
SET particleid = (
        SELECT p_dst.particleid
        FROM alnparticle p_src
        JOIN alnparticle p_dst
          ON p_dst.particlename = p_src.particlename
        WHERE p_src.particleid = shardinstance.particleid
          AND p_dst.schemaid IN (
              SELECT s_dst.schemaid
              FROM alnschema s_dst
              JOIN alnschema s_src
                ON s_dst.schemaname = s_src.schemaname
             WHERE s_src.schemaid = p_src.schemaid
          )
     )
WHERE particleid IN (
    SELECT p_src.particleid
    FROM alnparticle p_src
    JOIN alnschema s_src
      ON s_src.schemaid = p_src.schemaid
    JOIN src_repo
      ON s_src.repofileid IN (
             SELECT fileid
             FROM repofile
             WHERE repoid = (SELECT repoid FROM src_repo)
         )
);

-------------------------------------------------------------------------------
-- 4. Rebind repo identity for repofile rows
-------------------------------------------------------------------------------

UPDATE repofile
SET repoid = (SELECT repoid FROM dst_repo)
WHERE repoid = (SELECT repoid FROM src_repo);

COMMIT;

PRAGMA foreign_keys = ON;
