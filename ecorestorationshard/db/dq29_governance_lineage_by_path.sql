-- filename: dq29_governance_lineage_by_path.sql
-- destination: ecorestorationshard/db/dq29_governance_lineage_by_path.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

-- 29. Full governance spine lineage for a given file path.

-- Replace the literal path with any relpath of interest, e.g.
-- 'src/bin/restorationindextool.rs'.

SELECT
    rf.relpath                             AS filepath,
    rf.purpose                             AS file_purpose,
    rf.language                            AS file_language,
    dr.logicalname                         AS definition_logicalname,
    dr.versiontag                          AS definition_version,
    dr.status                              AS definition_status,
    dr.contractid                          AS definition_contractid,
    rib.bostromaddress                     AS identity_bostromaddress,
    rib.contractid                         AS identity_contractid,
    rib.dbrole                             AS identity_dbrole,
    rib.scope                              AS identity_scope,
    bcb.role                               AS contract_role,
    bcb.evidencehex                        AS contract_evidencehex
FROM repofile AS rf
JOIN repo AS r
  ON r.repoid = rf.repoid
LEFT JOIN definitionregistryrestoration AS dr
  ON dr.repofileid = rf.fileid
LEFT JOIN restorationidentitybinding AS rib
  ON rib.filepath  = rf.relpath
 AND rib.repotarget = 'github.commk-bluebirdecorestorationshard'
LEFT JOIN restorationcontract AS rc
  ON rc.logicalname = dr.logicalname
LEFT JOIN bostromcontractbinding AS bcb
  ON bcb.contractid = rc.contractid
WHERE r.name   = 'eco_restoration_shard'
  AND rf.relpath = 'src/bin/restorationindextool.rs';
