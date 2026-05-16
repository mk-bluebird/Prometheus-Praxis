-- filename: dq35_duplicate_logicalname_contracts.sql
-- destination: ecorestorationshard/db/dq35_duplicate_logicalname_contracts.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

-- 35. Detect duplicate logical_name values across different contracts.

-- Step 1: Find logicalnames associated with multiple distinct contract IDs.

WITH logicalname_contracts AS (
    SELECT
        logicalname,
        contractid,
        versiontag,
        status,
        region,
        addressid
    FROM restorationcontract
),
dupe_logicalnames AS (
    SELECT
        logicalname
    FROM logicalname_contracts
    GROUP BY logicalname
    HAVING COUNT(DISTINCT contractid) > 1
)
SELECT
    lc.logicalname,
    lc.contractid,
    lc.versiontag,
    lc.status,
    lc.region,
    lc.addressid
FROM logicalname_contracts AS lc
JOIN dupe_logicalnames AS d
  ON d.logicalname = lc.logicalname
ORDER BY lc.logicalname, lc.contractid;


-- Suggested remedial pattern (commented, to be adapted per case):

-- -- Example: migrate one duplicate to a new logicalname with explicit version:
-- INSERT INTO restorationcontract (
--     logicalname,
--     versiontag,
--     status,
--     region,
--     addressid
-- )
-- SELECT
--     logicalname || '.fix2026v1' AS logicalname,
--     '2026v1'                    AS versiontag,
--     status,
--     region,
--     addressid
-- FROM restorationcontract
-- WHERE contractid = :conflicting_contractid;
--
-- -- Then update downstream references (definitionregistryrestoration, 
-- -- restorationidentitybinding, bostromcontractbinding) to point at the new
-- -- logicalname and contractid, restoring one-to-one logicalname semantics.
