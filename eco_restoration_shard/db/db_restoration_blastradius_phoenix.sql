-- filename: db_restoration_blastradius_phoenix.sql
-- destination: eco_restoration_shard/db/db_restoration_blastradius_phoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard
-- logicalname: restoration.blastradius.phoenix.2026v1
-- author: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7

PRAGMA foreign_keys = ON;

-- =============================================================================
-- Restoration-Aware Blastradius Grammar for Phoenix
-- Extends blastradiusindex with restoration-specific fields and views
-- =============================================================================

-- Ensure base blastradiusindex table exists (or extend if already present)
CREATE TABLE IF NOT EXISTS blastradiusindex (
    blast_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    node_id          TEXT NOT NULL,
    region           TEXT NOT NULL DEFAULT 'PHOENIX-AZ',
    lane             TEXT,
    domain           TEXT,
    assetclass       TEXT,
    blast_radius_km  REAL NOT NULL DEFAULT 0.0,
    deltamass_window_kg REAL NOT NULL DEFAULT 0.0,
    deltakarma_window REAL NOT NULL DEFAULT 0.0,
    gw_risk_max      REAL NOT NULL DEFAULT 0.0,
    -- Restoration-specific extensions
    restorationradius_m REAL NOT NULL DEFAULT 0.0,  -- Restoration impact radius in meters
    restoration_ok   INTEGER NOT NULL DEFAULT 1 CHECK (restoration_ok IN (0, 1)),
    -- Authorship metadata
    author_bostrom   TEXT NOT NULL DEFAULT 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    createdutc       TEXT NOT NULL DEFAULT (datetime('now')),
    updatedutc       TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_blast_region ON blastradiusindex (region);
CREATE INDEX IF NOT EXISTS idx_blast_node ON blastradiusindex (node_id);
CREATE INDEX IF NOT EXISTS idx_blast_restoration_ok ON blastradiusindex (restoration_ok);
CREATE INDEX IF NOT EXISTS idx_blast_restorationradius ON blastradiusindex (restorationradius_m);
CREATE INDEX IF NOT EXISTS idx_blast_gw_risk ON blastradiusindex (gw_risk_max);

-- =============================================================================
-- Phoenix-scoped restoration blastradius view
-- Filters to Phoenix-AZ region with restoration-aware columns
-- =============================================================================

CREATE VIEW IF NOT EXISTS v_blastradius_restoration_phx AS
SELECT 
    blast_id,
    node_id,
    lane,
    domain,
    assetclass,
    blast_radius_km,
    deltamass_window_kg,
    deltakarma_window,
    gw_risk_max,
    restorationradius_m,
    restoration_ok,
    CASE 
        WHEN restoration_ok = 1 AND gw_risk_max < 0.5 THEN 'RESTORATION_SAFE'
        WHEN restoration_ok = 1 AND gw_risk_max >= 0.5 THEN 'RESTORATION_CAUTION'
        WHEN restoration_ok = 0 THEN 'RESTORATION_BLOCKED'
        ELSE 'UNKNOWN'
    END AS restoration_status,
    author_bostrom,
    createdutc,
    updatedutc
FROM blastradiusindex
WHERE region = 'PHOENIX-AZ'
ORDER BY node_id, blast_id;

-- =============================================================================
-- Phoenix restoration nodes view
-- Aggregates restoration metrics by node for Phoenix-AZ
-- =============================================================================

CREATE VIEW IF NOT EXISTS v_restoration_nodes_phx AS
SELECT 
    node_id,
    lane,
    domain,
    assetclass,
    COUNT(*) as blast_count,
    SUM(CASE WHEN restoration_ok = 1 THEN 1 ELSE 0 END) as restoration_approved_count,
    SUM(CASE WHEN restoration_ok = 0 THEN 1 ELSE 0 END) as restoration_blocked_count,
    AVG(restorationradius_m) as avg_restoration_radius_m,
    MAX(restorationradius_m) as max_restoration_radius_m,
    AVG(gw_risk_max) as avg_gw_risk,
    MAX(gw_risk_max) as max_gw_risk,
    SUM(deltamass_window_kg) as total_deltamass_kg,
    SUM(deltakarma_window) as total_deltakarma,
    CASE 
        WHEN SUM(CASE WHEN restoration_ok = 0 THEN 1 ELSE 0 END) = 0 THEN 'FULLY_RESTORABLE'
        WHEN SUM(CASE WHEN restoration_ok = 1 THEN 1 ELSE 0 END) > 0 THEN 'PARTIALLY_RESTORABLE'
        ELSE 'NON_RESTORABLE'
    END AS node_restoration_status
FROM blastradiusindex
WHERE region = 'PHOENIX-AZ'
GROUP BY node_id, lane, domain, assetclass
ORDER BY node_id;

-- =============================================================================
-- Sample data for Phoenix-AZ restoration blastradius
-- Demonstrates Cyboquatic MAR nodes with restoration metrics
-- =============================================================================

INSERT OR IGNORE INTO blastradiusindex (
    node_id, region, lane, domain, assetclass,
    blast_radius_km, deltamass_window_kg, deltakarma_window, gw_risk_max,
    restorationradius_m, restoration_ok, author_bostrom
) VALUES
-- Cyboquatic MAR node 1 - restoration approved
(
    'CYBOQUATIC-MAR-001',
    'PHOENIX-AZ',
    'ENERGY-DISTRIBUTION',
    'MARINE-INFRASTRUCTURE',
    'SUBSEA-CABLE',
    5.0,
    1000.0,
    0.1,
    0.2,
    50.0,
    1,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),
-- Cyboquatic MAR node 2 - restoration approved with higher risk
(
    'CYBOQUATIC-MAR-002',
    'PHOENIX-AZ',
    'ENERGY-DISTRIBUTION',
    'MARINE-INFRASTRUCTURE',
    'SUBSEA-CABLE',
    8.0,
    2500.0,
    0.3,
    0.4,
    75.0,
    1,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),
-- Phoenix datacenter node - restoration blocked due to high GW risk
(
    'PHX-DC-CENTRAL-001',
    'PHOENIX-AZ',
    'COMPUTE-CORE',
    'DATA-INFRASTRUCTURE',
    'DATACENTER',
    15.0,
    50000.0,
    2.5,
    0.8,
    0.0,
    0,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),
-- Solar farm node - restoration approved
(
    'PHX-SOLAR-FARM-001',
    'PHOENIX-AZ',
    'ENERGY-GENERATION',
    'RENEWABLE-INFRASTRUCTURE',
    'SOLAR-ARRAY',
    3.0,
    500.0,
    0.05,
    0.1,
    100.0,
    1,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

-- =============================================================================
-- Additional utility views
-- =============================================================================

-- View for high-risk nodes requiring attention
CREATE VIEW IF NOT EXISTS v_high_risk_restoration_nodes AS
SELECT 
    node_id,
    lane,
    domain,
    gw_risk_max,
    restorationradius_m,
    restoration_ok,
    CASE 
        WHEN gw_risk_max >= 0.7 THEN 'CRITICAL'
        WHEN gw_risk_max >= 0.5 THEN 'HIGH'
        ELSE 'MODERATE'
    END AS risk_level
FROM blastradiusindex
WHERE region = 'PHOENIX-AZ' AND gw_risk_max >= 0.3
ORDER BY gw_risk_max DESC;

-- View for restoration-ready nodes (low risk, approved)
CREATE VIEW IF NOT EXISTS v_restoration_ready_nodes AS
SELECT 
    node_id,
    lane,
    domain,
    assetclass,
    restorationradius_m,
    gw_risk_max,
    deltamass_window_kg,
    deltakarma_window
FROM blastradiusindex
WHERE region = 'PHOENIX-AZ' 
  AND restoration_ok = 1 
  AND gw_risk_max < 0.5
ORDER BY restorationradius_m DESC;
