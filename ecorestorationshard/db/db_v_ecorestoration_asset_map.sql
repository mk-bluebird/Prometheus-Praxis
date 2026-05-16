-- filename: db_v_ecorestoration_asset_map.sql
-- destination: ecorestorationshard/db/db_v_ecorestoration_asset_map.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 34. Materialized view table for cross-platform export.

DROP TABLE IF EXISTS v_ecorestoration_asset_map;

CREATE TABLE v_ecorestoration_asset_map AS
SELECT
    r.name                  AS repo_name,
    rf.relpath              AS filepath,
    rf.purpose              AS file_purpose,
    rf.language             AS file_language,
    dr.logicalname          AS definition_logicalname,
    dr.versiontag           AS definition_version,
    dr.status               AS definition_status,
    dr.contractid           AS definition_contractid,
    rc.contractid           AS restoration_contract_id,
    rc.status               AS restoration_contract_status,
    rc.region               AS restoration_contract_region,
    rc.addressid            AS restoration_contract_addressid,
    bcb.addressid           AS binding_addressid,
    bcb.role                AS binding_role,
    bcb.evidencehex         AS binding_evidencehex,
    rib.bostromaddress      AS identity_bostromaddress,
    rib.region              AS identity_region,
    rib.scope               AS identity_scope,
    rib.dbrole              AS identity_dbrole,
    rib.contractid          AS identity_contractid,
    rib.comment             AS identity_comment,
    rib.createdutc          AS identity_createdutc,
    rib.updatedutc          AS identity_updatedutc
FROM repo AS r
JOIN repofile AS rf
  ON rf.repoid = r.repoid
LEFT JOIN definitionregistryrestoration AS dr
  ON dr.repofileid = rf.fileid
LEFT JOIN restorationcontract AS rc
  ON rc.logicalname = dr.logicalname
LEFT JOIN bostromcontractbinding AS bcb
  ON bcb.contractid = rc.contractid
LEFT JOIN restorationidentitybinding AS rib
  ON rib.repotarget = 'github.commk-bluebirdecorestorationshard'
 AND rib.filepath   = rf.relpath;

CREATE INDEX IF NOT EXISTS idx_v_eam_logicalname
    ON v_ecorestoration_asset_map (definition_logicalname);

CREATE INDEX IF NOT EXISTS idx_v_eam_bostrom
    ON v_ecorestoration_asset_map (identity_bostromaddress);

-- Example export:
--   .mode csv
--   .headers on
--   SELECT * FROM v_ecorestoration_asset_map;
