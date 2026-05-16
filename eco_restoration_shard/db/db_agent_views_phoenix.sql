-- filename: db_agent_views_phoenix.sql
-- destination: eco_restoration_shard/db/db_agent_views_phoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard
-- logicalname: agent.views.phoenix.2026v1
-- author: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7

PRAGMA foreign_keys = ON;

-- Agent-facing views are defined as thin, read-only wrappers over existing
-- governance views and tables, with stable, agent-oriented schemas.

DROP VIEW IF EXISTS vagentrestorationnodesphx;

CREATE VIEW vagentrestorationnodesphx AS
SELECT
    node_id,
    lane,
    domain,
    assetclass,
    blast_radius_km,
    gw_risk_max,
    restorationradius_m,
    restoration_ok,
    restoration_status,
    author_bostrom,
    createdutc,
    updatedutc
FROM v_blastradius_restoration_phx;


DROP VIEW IF EXISTS vagentecoperjoulephx;

CREATE VIEW vagentecoperjoulephx AS
SELECT
    policy_id,
    region,
    lane,
    domain,
    assetclass,
    min_ecoperjoule,
    max_ecoperjoule,
    threshold_warning,
    threshold_critical,
    prod_eligible,
    eco_efficiency_score,
    energy_case,
    compliance_status,
    author_bostrom,
    createdutc,
    updatedutc
FROM v_cyboquatic_ecoperjoule_prod_phx
WHERE region = 'PHOENIX-AZ';


DROP VIEW IF EXISTS vagentmt6883continuityphx;

CREATE VIEW vagentmt6883continuityphx AS
SELECT
    lane_id,
    lane_name,
    region,
    domain,
    status,
    mt6883_ok,
    neuroethic_radius_hours,
    neuroethic_ok,
    capacity_pct,
    latency_ms,
    error_rate,
    continuity_status,
    neuroethic_review_category,
    author_bostrom,
    createdutc,
    updatedutc
FROM v_mt6883_lane_continuity
WHERE region = 'PHOENIX-AZ';


DROP VIEW IF EXISTS vagentgovernancesummaryphx;

CREATE VIEW vagentgovernancesummaryphx AS
SELECT 
    'RESTORATION_NODES' AS metric_category,
    COUNT(*) AS total_count,
    SUM(CASE WHEN restoration_ok = 1 THEN 1 ELSE 0 END) AS approved_count,
    AVG(gw_risk_max) AS avg_risk_score
FROM blastradiusindex
WHERE region = 'PHOENIX-AZ'
UNION ALL
SELECT 
    'ECOPERJOULE_WINDOWS' AS metric_category,
    COUNT(*) AS total_count,
    SUM(CASE WHEN prod_eligible = 1 THEN 1 ELSE 0 END) AS approved_count,
    AVG(min_ecoperjoule) AS avg_efficiency_score
FROM ecoperjoule_policy
WHERE region = 'PHOENIX-AZ'
UNION ALL
SELECT 
    'MT6883_LANES' AS metric_category,
    COUNT(*) AS total_count,
    SUM(CASE WHEN mt6883ok = 1 AND neuroethic_ok = 1 THEN 1 ELSE 0 END) AS approved_count,
    AVG(error_rate) AS avg_error_rate
FROM lanestatusshard
WHERE region = 'PHOENIX-AZ';


DROP VIEW IF EXISTS vagentdefinitionregistryphx;

CREATE VIEW vagentdefinitionregistryphx AS
SELECT 
    dr.logicalname      AS artifact_logical_name,
    dr.versiontag       AS version,
    dr.status           AS registration_status,
    dr.contractid       AS contract_id,
    rf.file_path        AS source_file,
    r.region            AS scoped_region,
    'REGION'            AS governance_scope
FROM definitionregistry_restoration AS dr
LEFT JOIN repofile AS rf
  ON rf.repofile_id = dr.repofileid
LEFT JOIN repo AS r
  ON r.repo_id = rf.repo_id
ORDER BY dr.artifact_type, dr.logicalname;


DROP VIEW IF EXISTS vagentidentitybindingsphx;

CREATE VIEW vagentidentitybindingsphx AS
SELECT 
    bostrom_address AS author_address,
    logicalname     AS bound_artifact,
    file_path       AS artifact_file,
    region          AS scoped_region,
    scope           AS governance_scope,
    dbrole          AS governance_role,
    role_description AS role_desc,
    createdutc      AS bound_at,
    updatedutc      AS updated_at
FROM restoration_identity_binding
WHERE region = 'PHOENIX-AZ'
ORDER BY dbrole, logicalname;
