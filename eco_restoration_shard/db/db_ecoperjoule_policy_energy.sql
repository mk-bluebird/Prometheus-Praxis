-- filename: db_ecoperjoule_policy_energy.sql
-- destination: eco_restoration_shard/db/db_ecoperjoule_policy_energy.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard
-- logicalname: energy.ecoperjoule.policy.phoenix.2026v1
-- author: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7

PRAGMA foreign_keys = ON;

-- =============================================================================
-- Eco-Per-Joule Policy Grammar for Phoenix Energy Planes
-- Defines ecoperjoule_policy table and Cyboquatic views for energy efficiency
-- =============================================================================

CREATE TABLE IF NOT EXISTS ecoperjoule_policy (
    policy_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    region           TEXT NOT NULL DEFAULT 'PHOENIX-AZ',
    lane             TEXT NOT NULL,          -- e.g., ENERGY-DISTRIBUTION, ENERGY-GENERATION
    domain           TEXT NOT NULL,          -- e.g., MARINE-INFRASTRUCTURE, RENEWABLE-INFRASTRUCTURE
    assetclass       TEXT NOT NULL,          -- e.g., SUBSEA-CABLE, SOLAR-ARRAY
    -- Eco-per-joule thresholds
    min_ecoperjoule  REAL NOT NULL DEFAULT 0.0,  -- Minimum acceptable eco-per-joule ratio
    max_ecoperjoule  REAL NOT NULL DEFAULT 1.0,  -- Maximum (ideal) eco-per-joule ratio
    threshold_warning REAL NOT NULL DEFAULT 0.3,  -- Warning threshold below ideal
    threshold_critical REAL NOT NULL DEFAULT 0.1, -- Critical threshold requiring action
    -- Production eligibility
    prod_eligible    INTEGER NOT NULL DEFAULT 1 CHECK (prod_eligible IN (0, 1)),
    -- Metadata
    description      TEXT NOT NULL,
    author_bostrom   TEXT NOT NULL DEFAULT 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    createdutc       TEXT NOT NULL DEFAULT (datetime('now')),
    updatedutc       TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE (region, lane, domain, assetclass)
);

CREATE INDEX IF NOT EXISTS idx_ecoperjoule_region ON ecoperjoule_policy (region);
CREATE INDEX IF NOT EXISTS idx_ecoperjoule_lane ON ecoperjoule_policy (lane);
CREATE INDEX IF NOT EXISTS idx_ecoperjoule_prod_eligible ON ecoperjoule_policy (prod_eligible);
CREATE INDEX IF NOT EXISTS idx_ecoperjoule_assetclass ON ecoperjoule_policy (assetclass);

-- =============================================================================
-- General Cyboquatic eco-per-joule view
-- Shows all policies with computed compliance status
-- =============================================================================

CREATE VIEW IF NOT EXISTS v_cyboquatic_ecoperjoule AS
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
    description,
    CASE 
        WHEN prod_eligible = 0 THEN 'NOT_ELIGIBLE'
        WHEN min_ecoperjoule >= threshold_critical AND min_ecoperjoule < threshold_warning THEN 'WARNING'
        WHEN min_ecoperjoule < threshold_critical THEN 'CRITICAL'
        ELSE 'COMPLIANT'
    END AS compliance_status,
    author_bostrom,
    createdutc,
    updatedutc
FROM ecoperjoule_policy
ORDER BY region, lane, domain, assetclass;

-- =============================================================================
-- Phoenix production-eligible eco-per-joule view
-- Filters to Phoenix-AZ production-eligible nodes with energy-case logic
-- =============================================================================

CREATE VIEW IF NOT EXISTS v_cyboquatic_ecoperjoule_prod_phx AS
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
    -- Computed eco-efficiency score using CASE logic
    CASE 
        -- High efficiency: meets or exceeds max target
        WHEN min_ecoperjoule >= max_ecoperjoule THEN 100.0
        -- Good efficiency: above warning threshold
        WHEN min_ecoperjoule >= threshold_warning THEN 
            ROUND(50.0 + ((min_ecoperjoule - threshold_warning) / (max_ecoperjoule - threshold_warning)) * 50.0, 2)
        -- Low efficiency: between critical and warning
        WHEN min_ecoperjoule >= threshold_critical THEN 
            ROUND(25.0 + ((min_ecoperjoule - threshold_critical) / (threshold_warning - threshold_critical)) * 25.0, 2)
        -- Critical: below critical threshold
        ELSE 
            ROUND((min_ecoperjoule / threshold_critical) * 25.0, 2)
    END AS eco_efficiency_score,
    -- Energy case classification
    CASE 
        WHEN prod_eligible = 0 THEN 'NON_PRODUCTION'
        WHEN min_ecoperjoule >= max_ecoperjoule THEN 'OPTIMAL_ENERGY_CASE'
        WHEN min_ecoperjoule >= threshold_warning THEN 'ACCEPTABLE_ENERGY_CASE'
        WHEN min_ecoperjoule >= threshold_critical THEN 'MARGINAL_ENERGY_CASE'
        ELSE 'CRITICAL_ENERGY_CASE'
    END AS energy_case,
    description,
    author_bostrom,
    createdutc,
    updatedutc
FROM ecoperjoule_policy
WHERE region = 'PHOENIX-AZ' AND prod_eligible = 1
ORDER BY eco_efficiency_score DESC;

-- =============================================================================
-- Insert Phoenix-AZ eco-per-joule policies
-- Focus on Cyboquatic MAR nodes and renewable infrastructure
-- =============================================================================

INSERT OR REPLACE INTO ecoperjoule_policy (
    region, lane, domain, assetclass,
    min_ecoperjoule, max_ecoperjoule, threshold_warning, threshold_critical,
    prod_eligible, description, author_bostrom
) VALUES
-- Cyboquatic MAR subsea cable - high efficiency target
(
    'PHOENIX-AZ',
    'ENERGY-DISTRIBUTION',
    'MARINE-INFRASTRUCTURE',
    'SUBSEA-CABLE',
    0.85,
    0.95,
    0.6,
    0.3,
    1,
    'Cyboquatic MAR subsea cable eco-per-joule policy targeting high energy efficiency for underwater power distribution.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),
-- Solar farm array - optimal renewable target
(
    'PHOENIX-AZ',
    'ENERGY-GENERATION',
    'RENEWABLE-INFRASTRUCTURE',
    'SOLAR-ARRAY',
    0.90,
    0.98,
    0.7,
    0.4,
    1,
    'Phoenix solar array eco-per-joule policy ensuring maximum renewable energy conversion efficiency.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),
-- Wind turbine farm - variable efficiency target
(
    'PHOENIX-AZ',
    'ENERGY-GENERATION',
    'RENEWABLE-INFRASTRUCTURE',
    'WIND-TURBINE',
    0.75,
    0.92,
    0.55,
    0.25,
    1,
    'Phoenix wind turbine eco-per-joule policy accounting for variable wind conditions.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),
-- Datacenter compute - lower efficiency but critical load
(
    'PHOENIX-AZ',
    'COMPUTE-CORE',
    'DATA-INFRASTRUCTURE',
    'DATACENTER',
    0.45,
    0.70,
    0.35,
    0.2,
    1,
    'Phoenix datacenter eco-per-joule policy balancing compute needs with energy efficiency.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),
-- Legacy grid connection - marginal efficiency, phase-out candidate
(
    'PHOENIX-AZ',
    'ENERGY-DISTRIBUTION',
    'GRID-INFRASTRUCTURE',
    'LEGACY-GRID',
    0.25,
    0.50,
    0.3,
    0.15,
    0,
    'Legacy grid connection marked non-production eligible due to low eco-per-joule efficiency.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

-- =============================================================================
-- Additional utility views
-- =============================================================================

-- View for non-compliant policies requiring attention
CREATE VIEW IF NOT EXISTS v_ecoperjoule_non_compliant AS
SELECT 
    policy_id,
    region,
    lane,
    domain,
    assetclass,
    min_ecoperjoule,
    threshold_warning,
    threshold_critical,
    CASE 
        WHEN min_ecoperjoule < threshold_critical THEN 'CRITICAL_NON_COMPLIANCE'
        WHEN min_ecoperjoule < threshold_warning THEN 'WARNING_NON_COMPLIANCE'
        ELSE 'UNKNOWN'
    END AS non_compliance_level,
    description
FROM ecoperjoule_policy
WHERE prod_eligible = 1 AND min_ecoperjoule < threshold_warning
ORDER BY min_ecoperjoule ASC;

-- View summarizing eco-per-joule by lane
CREATE VIEW IF NOT EXISTS v_ecoperjoule_summary_by_lane AS
SELECT 
    lane,
    COUNT(*) as policy_count,
    AVG(min_ecoperjoule) as avg_min_ecoperjoule,
    AVG(max_ecoperjoule) as avg_max_ecoperjoule,
    SUM(CASE WHEN prod_eligible = 1 THEN 1 ELSE 0 END) as production_eligible_count,
    SUM(CASE WHEN min_ecoperjoule >= threshold_warning THEN 1 ELSE 0 END) as compliant_count
FROM ecoperjoule_policy
WHERE region = 'PHOENIX-AZ'
GROUP BY lane
ORDER BY lane;
