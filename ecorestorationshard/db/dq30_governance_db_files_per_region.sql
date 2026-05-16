-- filename: dq30_governance_db_files_per_region.sql
-- destination: ecorestorationshard/db/dq30_governance_db_files_per_region.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

-- 30. Count of governance-DB files per region bound to primary Bostrom address.

SELECT
    rib.region,
    COUNT(*) AS governance_db_files
FROM restorationidentitybinding AS rib
WHERE rib.bostromaddress = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
  AND rib.dbrole         = 'GOVERNANCE_DB'
GROUP BY rib.region
ORDER BY rib.region;
