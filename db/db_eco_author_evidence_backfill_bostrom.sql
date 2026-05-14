-- filename db_eco_author_evidence_backfill_bostrom.sql
-- destination eco_restoration_shard/db/db_eco_author_evidence_backfill_bostrom.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 47. Backfill eco_author_evidence for primary Bostrom address
-------------------------------------------------------------------------------
-- Assumptions:
--   shardinstance(signingdid, shardid, ...)
--   eco_author_evidence(evidence_id, shardid, signingdid, legacy_login, current_login, authored_utc, source_kind, note)
--   Primary Bostrom DID: 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'.
--   Legacy login to attribute: 'Doctor0Evil'.
--
-- Idempotent: uses NOT EXISTS guard.

WITH candidate_shards AS (
    SELECT
        si.shardid,
        si.signingdid,
        si.tstartutc AS authored_utc
    FROM shardinstance si
    WHERE si.signingdid = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),
missing_evidence AS (
    SELECT
        cs.shardid,
        cs.signingdid,
        cs.authored_utc
    FROM candidate_shards cs
    WHERE NOT EXISTS (
        SELECT 1
        FROM eco_author_evidence ae
        WHERE ae.shardid = cs.shardid
          AND ae.signingdid = cs.signingdid
    )
)
INSERT INTO eco_author_evidence (
    shardid,
    signingdid,
    legacy_login,
    current_login,
    authored_utc,
    source_kind,
    note
)
SELECT
    me.shardid,
    me.signingdid,
    'Doctor0Evil'          AS legacy_login,
    'mk-bluebird'          AS current_login,
    COALESCE(me.authored_utc, datetime('now')) AS authored_utc,
    'LEGACY_IMPORT'        AS source_kind,
    'Backfilled from shardinstance.signingdid for primary Bostrom address.' AS note
FROM missing_evidence me;
