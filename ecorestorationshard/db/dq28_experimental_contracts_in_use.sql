-- filename: dq28_experimental_contracts_in_use.sql
-- destination: ecorestorationshard/db/dq28_experimental_contracts_in_use.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

-- 28. Experimental restoration_contract entries referenced by at least one
--     restorationidentitybinding row.

SELECT DISTINCT
    rc.contractid,
    rc.logicalname,
    rc.versiontag,
    rc.status,
    rc.region,
    rc.addressid
FROM restorationcontract AS rc
JOIN bostromcontractbinding AS bcb
  ON bcb.contractid = rc.contractid
JOIN restorationidentitybinding AS rib
  ON rib.contractid = rc.logicalname
WHERE rc.status = 'EXPERIMENTAL';
