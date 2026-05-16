-- filename: dq27_missing_identity_bindings.sql
-- destination: ecorestorationshard/db/dq27_missing_identity_bindings.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

-- 27. Files in eco_restoration_shard with no restorationidentitybinding
--     for the primary Bostrom address.

SELECT
    r.name              AS repo_name,
    rf.relpath          AS filepath,
    rf.purpose,
    rf.language
FROM repo      AS r
JOIN repofile  AS rf
  ON rf.repoid = r.repoid
LEFT JOIN restorationidentitybinding AS rib
  ON rib.repotarget     = 'github.commk-bluebirdecorestorationshard'
 AND rib.filepath       = rf.relpath
 AND rib.bostromaddress = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
WHERE r.name = 'eco_restoration_shard'
  AND rib.bindingid IS NULL
ORDER BY rf.relpath;
