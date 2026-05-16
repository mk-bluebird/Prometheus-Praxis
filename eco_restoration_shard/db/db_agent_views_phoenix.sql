-- filename: db_agent_views_phoenix.sql
-- destination: ecorestorationshard/db/db_agent_views_phoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard
-- logicalname: agent.views.phoenix.2026v1
-- author: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7

PRAGMA foreign_keys = ON;

-- Agent-facing views are defined as thin, read-only wrappers over existing
-- governance views and tables, with stable, agent-oriented schemas.

DROP VIEW IF EXISTS vagentrestorationnodesphx;

CREATE VIEW vagentrestorationnodesphx AS
SELECT
    region,
    domain,
    scopeid,
    planeid,
    restorationradiusm,
    restorationradiushours,
    deltamasswindowkg,
    deltakarmawindow,
    gwriskmax,
    restorationok,
    authorbostrom,
    authorcontractid,
    createdutc,
    updatedutc
FROM vrestorationnodesphx
WHERE region = 'Phoenix-AZ';


DROP VIEW IF EXISTS vagentecoperjoulephx;

CREATE VIEW vagentecoperjoulephx AS
SELECT
    nodeid,
    region,
    domain,
    twindowstart,
    twindowend,
    vtresidual,
    kscore,
    escore,
    rscore,
    lane,
    kerdeployable,
    ecoperjoule,
    thetaecomin,
    carbonnegativeok,
    authorbostrom,
    authorcontractid
FROM vcyboquaticecoperjouleprodphx
WHERE region = 'Phoenix-AZ';


DROP VIEW IF EXISTS vagentmt6883continuityphx;

CREATE VIEW vagentmt6883continuityphx AS
SELECT
    kernelid,
    region,
    lane,
    kscore,
    escore,
    rscore,
    vtmax,
    planesok,
    topologyok,
    mt6883registryid,
    mt6883ok,
    neuroethicradiushours,
    neuroethicok,
    authorbostrom,
    authorcontractid
FROM vmt6883lanecontinuity
WHERE region = 'Phoenix-AZ';


DROP VIEW IF EXISTS vagentgovernancesummaryphx;

CREATE VIEW vagentgovernancesummaryphx AS
SELECT 
    'RESTORATION_NODES' AS metric_category,
    COUNT(*) AS total_count,
    SUM(CASE WHEN restorationok = 1 THEN 1 ELSE 0 END) AS approved_count,
    AVG(gwriskmax) AS avg_risk_score
FROM vrestorationnodesphx
WHERE region = 'Phoenix-AZ'
UNION ALL
SELECT 
    'ECOPERJOULE_WINDOWS' AS metric_category,
    COUNT(*) AS total_count,
    SUM(CASE WHEN carbonnegativeok = 1 THEN 1 ELSE 0 END) AS approved_count,
    AVG(ecoperjoule) AS avg_efficiency_score
FROM vcyboquaticecoperjouleprodphx
WHERE region = 'Phoenix-AZ'
UNION ALL
SELECT 
    'MT6883_LANES' AS metric_category,
    COUNT(*) AS total_count,
    SUM(CASE WHEN mt6883ok = 1 AND neuroethicok = 1 THEN 1 ELSE 0 END) AS approved_count,
    AVG(vtmax) AS avg_error_proxy
FROM vmt6883lanecontinuity
WHERE region = 'Phoenix-AZ';


DROP VIEW IF EXISTS vagentdefinitionregistryphx;

CREATE VIEW vagentdefinitionregistryphx AS
SELECT 
    dr.logicalname      AS artifact_logical_name,
    dr.versiontag       AS version,
    dr.status           AS registration_status,
    dr.contractid       AS contract_id,
    rf.relpath          AS source_file,
    r.region            AS scoped_region,
    'REGION'            AS governance_scope
FROM definitionregistryrestoration AS dr
JOIN repofile AS rf
  ON rf.fileid = dr.repofileid
JOIN repo AS r
  ON r.repoid = rf.repoid
WHERE r.name = 'eco_restoration_shard';


DROP VIEW IF EXISTS vagentidentitybindingsphx;

CREATE VIEW vagentidentitybindingsphx AS
SELECT 
    bostromaddress AS author_address,
    logicalname    AS bound_artifact,
    filepath       AS artifact_file,
    region         AS scoped_region,
    scope          AS governance_scope,
    dbrole         AS governance_role,
    contractid     AS contract_id,
    createdutc     AS bound_at,
    updatedutc     AS updated_at
FROM restorationidentitybinding
WHERE region = 'Phoenix-AZ';
