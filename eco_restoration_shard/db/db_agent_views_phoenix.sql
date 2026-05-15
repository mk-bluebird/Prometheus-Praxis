-- filename: db_agent_views_phoenix.sql
-- destination: eco_restoration_shard/db/db_agent_views_phoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard
-- logicalname: agent.views.phoenix.2026v1
-- author: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7

PRAGMA foreign_keys = ON;

-- =============================================================================
-- Agent-Facing Governance Views for Phoenix
-- Provides stable, agent-oriented schemas for AI agents to query
-- KER, lanes, blast radius, ecoperjoule, and MT6883 continuity data
-- =============================================================================

-- -----------------------------------------------------------------------------
-- v_agent_restoration_nodes_phx
-- Agent-friendly view of restoration nodes in Phoenix-AZ
-- Simplified column names and status indicators for AI consumption
-- -----------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_agent_restoration_nodes_phx AS
SELECT 
    node_id AS node,
    lane AS operational_lane,
    domain AS service_domain,
    assetclass AS asset_type,
    blast_radius_km AS impact_radius_km,
    gw_risk_max AS gravitational_wave_risk,
    restorationradius_m AS restoration_impact_m,
    CASE 
        WHEN restoration_ok = 1 AND gw_risk_max < 0.5 THEN 'APPROVED_FOR_RESTORATION'
        WHEN restoration_ok = 1 AND gw_risk_max >= 0.5 THEN 'CAUTION_RESTORATION'
        WHEN restoration_ok = 0 THEN 'RESTORATION_NOT_APPROVED'
        ELSE 'STATUS_UNKNOWN'
    END AS restoration_approval_status,
    ROUND(gw_risk_max * 100, 2) AS risk_score_pct,
    ROUND(restorationradius_m, 2) AS restoration_radius_m
FROM blastradiusindex
WHERE region = 'PHOENIX-AZ'
ORDER BY node_id;

-- -----------------------------------------------------------------------------
-- v_agent_ecoperjoule_phx
-- Agent-friendly view of eco-per-joule policies for Phoenix production nodes
-- Provides energy efficiency metrics in agent-readable format
-- -----------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_agent_ecoperjoule_phx AS
SELECT 
    policy_id AS policy_identifier,
    lane AS operational_lane,
    domain AS service_domain,
    assetclass AS asset_type,
    min_ecoperjoule AS current_efficiency_ratio,
    max_ecoperjoule AS target_efficiency_ratio,
    threshold_warning AS warning_threshold,
    threshold_critical AS critical_threshold,
    CASE 
        WHEN min_ecoperjoule >= max_ecoperjoule THEN 'OPTIMAL'
        WHEN min_ecoperjoule >= threshold_warning THEN 'ACCEPTABLE'
        WHEN min_ecoperjoule >= threshold_critical THEN 'MARGINAL'
        ELSE 'CRITICAL'
    END AS efficiency_status,
    ROUND(min_ecoperjoule * 100, 2) AS efficiency_score_pct,
    prod_eligible = 1 AS is_production_eligible,
    description AS policy_description
FROM ecoperjoule_policy
WHERE region = 'PHOENIX-AZ'
ORDER BY min_ecoperjoule DESC;

-- -----------------------------------------------------------------------------
-- v_agent_mt6883_continuity_phx
-- Agent-friendly view of MT6883 lane continuity for Phoenix-AZ
-- Provides continuity status and neuroethic compliance metrics
-- -----------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_agent_mt6883_continuity_phx AS
SELECT 
    lane_id AS lane_identifier,
    lane_name AS lane,
    domain AS service_domain,
    status AS operational_status,
    mt6883_ok = 1 AS mt6883_continuity_verified,
    neuroethic_ok = 1 AS neuroethic_compliance_verified,
    neuroethic_radius_hours AS neuroethic_review_window_hrs,
    capacity_pct AS current_capacity_pct,
    latency_ms AS avg_latency_ms,
    error_rate AS current_error_rate,
    CASE 
        WHEN mt6883_ok = 1 AND neuroethic_ok = 1 AND status = 'ACTIVE' THEN 'FULL_CONTINUITY'
        WHEN mt6883_ok = 1 AND neuroethic_ok = 1 AND status = 'DEGRADED' THEN 'DEGRADED_CONTINUITY'
        WHEN mt6883_ok = 0 OR neuroethic_ok = 0 THEN 'CONTINUITY_FAILED'
        ELSE 'CONTINUITY_UNKNOWN'
    END AS continuity_status,
    CASE 
        WHEN error_rate > 0.1 THEN 'CRITICAL_ERRORS'
        WHEN error_rate > 0.05 THEN 'ELEVATED_ERRORS'
        WHEN error_rate > 0.01 THEN 'NORMAL_ERRORS'
        ELSE 'LOW_ERRORS'
    END AS error_status
FROM lanestatusshard
WHERE region = 'PHOENIX-AZ'
ORDER BY lane_name;

-- -----------------------------------------------------------------------------
-- v_agent_governance_summary_phx
-- Consolidated governance summary for Phoenix-AZ agents
-- Aggregates key metrics across all governance domains
-- -----------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_agent_governance_summary_phx AS
SELECT 
    'RESTORATION_NODES' AS metric_category,
    COUNT(*) AS total_count,
    SUM(CASE WHEN restoration_ok = 1 THEN 1 ELSE 0 END) AS approved_count,
    AVG(gw_risk_max) AS avg_risk_score
FROM blastradiusindex WHERE region = 'PHOENIX-AZ'
UNION ALL
SELECT 
    'ECOPERJOULE_POLICIES' AS metric_category,
    COUNT(*) AS total_count,
    SUM(CASE WHEN prod_eligible = 1 THEN 1 ELSE 0 END) AS approved_count,
    AVG(min_ecoperjoule) AS avg_efficiency_score
FROM ecoperjoule_policy WHERE region = 'PHOENIX-AZ'
UNION ALL
SELECT 
    'MT6883_LANES' AS metric_category,
    COUNT(*) AS total_count,
    SUM(CASE WHEN mt6883_ok = 1 AND neuroethic_ok = 1 THEN 1 ELSE 0 END) AS approved_count,
    AVG(error_rate) AS avg_error_rate
FROM lanestatusshard WHERE region = 'PHOENIX-AZ';

-- -----------------------------------------------------------------------------
-- v_agent_active_contracts_primary
-- Agent view of active restoration contracts (placeholder structure)
-- To be populated with actual contract data when available
-- -----------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_agent_active_contracts_primary AS
SELECT 
    'PLACEHOLDER' AS contract_id,
    'PHOENIX-AZ' AS region,
    'RESTORATION' AS contract_type,
    'ACTIVE' AS contract_status,
    datetime('now') AS last_updated,
    'Query DefinitionRegistry for actual contract bindings' AS note;

-- -----------------------------------------------------------------------------
-- v_agent_prod_eligible_restoration_planes
-- Agent view of production-eligible restoration planes
-- Combines restoration approval with production eligibility
-- -----------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_agent_prod_eligible_restoration_planes AS
SELECT 
    b.node_id AS plane_identifier,
    b.lane AS operational_lane,
    b.domain AS service_domain,
    b.assetclass AS asset_type,
    b.restorationradius_m AS restoration_impact_m,
    b.gw_risk_max AS risk_score,
    e.prod_eligible AS production_eligible,
    e.min_ecoperjoule AS efficiency_ratio,
    CASE 
        WHEN b.restoration_ok = 1 AND e.prod_eligible = 1 THEN 'PROD_ELIGIBLE'
        WHEN b.restoration_ok = 1 AND e.prod_eligible = 0 THEN 'RESTORATION_ONLY'
        WHEN b.restoration_ok = 0 THEN 'NOT_ELIGIBLE'
        ELSE 'STATUS_UNKNOWN'
    END AS eligibility_status
FROM blastradiusindex b
LEFT JOIN ecoperjoule_policy e ON b.lane = e.lane AND b.domain = e.domain AND b.assetclass = e.assetclass
WHERE b.region = 'PHOENIX-AZ'
ORDER BY 
    CASE WHEN b.restoration_ok = 1 AND e.prod_eligible = 1 THEN 0 ELSE 1 END,
    b.node_id;

-- -----------------------------------------------------------------------------
-- v_agent_definition_registry
-- Agent-friendly view of registered definitions
-- Provides logicalname to file_path mapping for artifact discovery
-- -----------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_agent_definition_registry AS
SELECT 
    logicalname AS artifact_logical_name,
    versiontag AS version,
    artifact_type AS type,
    file_path AS source_file,
    region AS scoped_region,
    scope AS governance_scope,
    status AS registration_status,
    description AS artifact_description,
    author_bostrom AS author_address,
    registered_utc AS registered_at
FROM definitionregistry_restoration
WHERE status IN ('ACTIVE', 'REGISTERED')
ORDER BY artifact_type, logicalname;

-- -----------------------------------------------------------------------------
-- v_agent_identity_bindings
-- Agent-friendly view of identity bindings
-- Allows agents to verify authorship and binding status
-- -----------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_agent_identity_bindings AS
SELECT 
    bostrom_address AS author_address,
    logicalname AS bound_artifact,
    file_path AS artifact_file,
    region AS scoped_region,
    scope AS governance_scope,
    dbrole AS governance_role,
    role_description AS role_summary,
    createdutc AS bound_at
FROM restoration_identity_binding
ORDER BY governance_role, bound_artifact;
