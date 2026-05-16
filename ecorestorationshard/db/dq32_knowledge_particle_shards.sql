-- filename: dq32_knowledge_particle_shards.sql
-- destination: ecorestorationshard/db/dq32_knowledge_particle_shards.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

-- 32. Knowledge-particle shard references and bindings to primary Bostrom address.

SELECT
    rf.relpath              AS filepath,
    rf.purpose,
    rf.language,
    rib.bostromaddress,
    rib.region,
    rib.scope,
    rib.dbrole,
    rib.contractid,
    rib.comment,
    rib.createdutc,
    rib.updatedutc
FROM repo      AS r
JOIN repofile  AS rf
  ON rf.repoid = r.repoid
LEFT JOIN restorationidentitybinding AS rib
  ON rib.repotarget     = 'github.commk-bluebirdecorestorationshard'
 AND rib.filepath       = rf.relpath
WHERE r.name   = 'eco_restoration_shard'
  AND rf.purpose IN ('DATA_SHARD', 'KNOWLEDGE_FRAGMENT')
  AND rib.bostromaddress = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
ORDER BY rf.relpath;
