-- filename: db_mt6883_lane_continuity.sql
-- destination: eco_restoration_shard/db/db_mt6883_lane_continuity.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard
-- logicalname: mt6883.lane.continuity.phoenix.2026v1
-- author: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7

PRAGMA foreign_keys = ON;

-- =============================================================================
-- MT6883 Lane Continuity and Neuroethic Radii Integration
-- Extends lanestatusshard and blastradiusindex with MT6883-specific fields
-- =============================================================================

-- Ensure lanestatusshard table exists with MT6883 extensions
CREATE TABLE IF NOT EXISTS lanestatusshard (
    lane_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    lane_name        TEXT NOT NULL UNIQUE,
    region           TEXT NOT NULL DEFAULT 'PHOENIX-AZ',
    domain           TEXT NOT NULL,
    status           TEXT NOT NULL DEFAULT 'ACTIVE',  -- ACTIVE, DEGRADED, OFFLINE, MAINTENANCE
    -- MT6883 continuity fields
    mt6883_ok        INTEGER NOT NULL DEFAULT 1 CHECK (mt6883_ok IN (0, 1)),
    neuroethic_radius_hours REAL NOT NULL DEFAULT 0.0,  -- Neuroethic review window in hours
    neuroethic_ok    INTEGER NOT NULL DEFAULT 1 CHECK (neuroethic_ok IN (0, 1)),
    -- Standard lane metrics
    capacity_pct     REAL NOT NULL DEFAULT 0.0,
    latency_ms       REAL NOT NULL DEFAULT 0.0,
    error_rate       REAL NOT NULL DEFAULT 0.0,
    -- Metadata
    author_bostrom   TEXT NOT NULL DEFAULT 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    createdutc       TEXT NOT NULL DEFAULT (datetime('now')),
    updatedutc       TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_lane_region ON lanestatusshard (region);
CREATE INDEX IF NOT EXISTS idx_lane_status ON lanestatusshard (status);
CREATE INDEX IF NOT EXISTS idx_lane_mt6883_ok ON lanestatusshard (mt6883_ok);
CREATE INDEX IF NOT EXISTS idx_lane_neuroethic_ok ON lanestatusshard (neuroethic_ok);

-- =============================================================================
-- Extend blastradiusindex with MT6883 fields if not already present
-- (Assumes base blastradiusindex from db_restoration_blastradius_phoenix.sql)
-- =============================================================================

-- Add MT6883 columns to blastradiusindex if they don't exist
-- Note: In production, use ALTER TABLE; here we ensure schema completeness
PRAGMA foreign_keys = OFF;

-- Create a view that represents the extended blastradiusindex with MT6883
-- This handles the case where the table may have been created without MT6883 fields
CREATE VIEW IF NOT EXISTS v_blastradiusindex_mt6883_extended AS
SELECT 
    blast_id,
    node_id,
    region,
    lane,
    domain,
    assetclass,
    blast_radius_km,
    deltamass_window_kg,
    deltakarma_window,
    gw_risk_max,
    restorationradius_m,
    restoration_ok,
    -- Default MT6883 values (would be actual columns in extended schema)
    1 AS mt6883_ok,
    0.0 AS neuroethic_radius_hours,
    1 AS neuroethic_ok,
    author_bostrom,
    createdutc,
    updatedutc
FROM blastradiusindex;

-- =============================================================================
-- MT6883 Lane Continuity View
-- Shows all lanes with MT6883 and neuroethic status
-- =============================================================================

CREATE VIEW IF NOT EXISTS v_mt6883_lane_continuity AS
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
    -- Computed continuity status
    CASE 
        WHEN mt6883_ok = 0 THEN 'MT6883_FAILED'
        WHEN neuroethic_ok = 0 THEN 'NEUROETHIC_FAILED'
        WHEN status = 'OFFLINE' THEN 'OFFLINE'
        WHEN status = 'MAINTENANCE' THEN 'MAINTENANCE'
        WHEN status = 'DEGRADED' THEN 'DEGRADED_CONTINUITY'
        WHEN error_rate > 0.05 THEN 'HIGH_ERROR_RATE'
        ELSE 'FULL_CONTINUITY'
    END AS continuity_status,
    -- Neuroethic compliance window
    CASE 
        WHEN neuroethic_radius_hours >= 72 THEN 'EXTENDED_REVIEW'
        WHEN neuroethic_radius_hours >= 24 THEN 'STANDARD_REVIEW'
        WHEN neuroethic_radius_hours >= 4 THEN 'RAPID_REVIEW'
        ELSE 'IMMEDIATE_ACTION'
    END AS neuroethic_review_category,
    author_bostrom,
    createdutc,
    updatedutc
FROM lanestatusshard
ORDER BY region, lane_name;

-- =============================================================================
-- Blastradius MT6883 Neuroethic View
-- Joins blast radius data with MT6883/neuroethic status
-- =============================================================================

CREATE VIEW IF NOT EXISTS v_blastradius_mt6883_neuro AS
SELECT 
    b.blast_id,
    b.node_id,
    b.region,
    b.lane,
    b.domain,
    b.assetclass,
    b.blast_radius_km,
    b.gw_risk_max,
    b.restorationradius_m,
    b.restoration_ok,
    l.lane_id,
    l.status AS lane_status,
    l.mt6883_ok,
    l.neuroethic_radius_hours,
    l.neuroethic_ok,
    l.error_rate,
    -- Combined risk assessment
    CASE 
        WHEN b.restoration_ok = 0 OR l.mt6883_ok = 0 OR l.neuroethic_ok = 0 THEN 'BLOCKED'
        WHEN b.gw_risk_max >= 0.7 OR l.error_rate > 0.1 THEN 'HIGH_RISK'
        WHEN b.gw_risk_max >= 0.5 OR l.error_rate > 0.05 THEN 'MODERATE_RISK'
        ELSE 'ACCEPTABLE_RISK'
    END AS combined_risk_status,
    -- Continuity score (0-100)
    ROUND(
        (CASE WHEN b.restoration_ok = 1 THEN 25.0 ELSE 0.0 END +
         CASE WHEN l.mt6883_ok = 1 THEN 25.0 ELSE 0.0 END +
         CASE WHEN l.neuroethic_ok = 1 THEN 25.0 ELSE 0.0 END +
         CASE WHEN l.status = 'ACTIVE' THEN 25.0 ELSE 0.0 END),
        2
    ) AS continuity_score
FROM blastradiusindex b
LEFT JOIN lanestatusshard l ON b.lane = l.lane_name AND b.region = l.region
WHERE b.region = 'PHOENIX-AZ'
ORDER BY continuity_score DESC, b.node_id;

-- =============================================================================
-- Insert Phoenix-AZ lane status with MT6883 continuity data
-- =============================================================================

INSERT OR REPLACE INTO lanestatusshard (
    lane_name, region, domain, status,
    mt6883_ok, neuroethic_radius_hours, neuroethic_ok,
    capacity_pct, latency_ms, error_rate, author_bostrom
) VALUES
-- Energy distribution lane - full continuity
(
    'ENERGY-DISTRIBUTION',
    'PHOENIX-AZ',
    'MARINE-INFRASTRUCTURE',
    'ACTIVE',
    1,
    48.0,
    1,
    75.5,
    12.3,
    0.01,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),
-- Energy generation lane - full continuity
(
    'ENERGY-GENERATION',
    'PHOENIX-AZ',
    'RENEWABLE-INFRASTRUCTURE',
    'ACTIVE',
    1,
    24.0,
    1,
    82.0,
    8.5,
    0.005,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),
-- Compute core lane - degraded but MT6883 OK
(
    'COMPUTE-CORE',
    'PHOENIX-AZ',
    'DATA-INFRASTRUCTURE',
    'DEGRADED',
    1,
    72.0,
    1,
    95.0,
    45.2,
    0.03,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),
-- Grid infrastructure lane - MT6883 failed, requires review
(
    'GRID-INFRASTRUCTURE',
    'PHOENIX-AZ',
    'LEGACY-INFRASTRUCTURE',
    'MAINTENANCE',
    0,
    168.0,
    0,
    30.0,
    120.5,
    0.08,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),
-- Communications lane - active with rapid neuroethic review
(
    'COMMUNICATIONS',
    'PHOENIX-AZ',
    'NETWORK-INFRASTRUCTURE',
    'ACTIVE',
    1,
    4.0,
    1,
    60.0,
    5.2,
    0.002,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

-- =============================================================================
-- Additional utility views
-- =============================================================================

-- View for lanes requiring MT6883 attention
CREATE VIEW IF NOT EXISTS v_mt6883_attention_required AS
SELECT 
    lane_id,
    lane_name,
    region,
    domain,
    mt6883_ok,
    neuroethic_ok,
    neuroethic_radius_hours,
    status,
    CASE 
        WHEN mt6883_ok = 0 AND neuroethic_ok = 0 THEN 'CRITICAL_MT6883_NEUROETHIC_FAILURE'
        WHEN mt6883_ok = 0 THEN 'CRITICAL_MT6883_FAILURE'
        WHEN neuroethic_ok = 0 THEN 'CRITICAL_NEUROETHIC_FAILURE'
        WHEN neuroethic_radius_hours > 72 THEN 'EXTENDED_REVIEW_NEEDED'
        ELSE 'MONITOR'
    END AS attention_level
FROM lanestatusshard
WHERE mt6883_ok = 0 OR neuroethic_ok = 0 OR neuroethic_radius_hours > 72
ORDER BY 
    CASE 
        WHEN mt6883_ok = 0 AND neuroethic_ok = 0 THEN 1
        WHEN mt6883_ok = 0 THEN 2
        WHEN neuroethic_ok = 0 THEN 3
        ELSE 4
    END;

-- View summarizing MT6883 continuity by region
CREATE VIEW IF NOT EXISTS v_mt6883_summary_by_region AS
SELECT 
    region,
    COUNT(*) as total_lanes,
    SUM(CASE WHEN mt6883_ok = 1 THEN 1 ELSE 0 END) as mt6883_ok_count,
    SUM(CASE WHEN neuroethic_ok = 1 THEN 1 ELSE 0 END) as neuroethic_ok_count,
    SUM(CASE WHEN status = 'ACTIVE' THEN 1 ELSE 0 END) as active_count,
    AVG(neuroethic_radius_hours) as avg_neuroethic_radius_hours,
    AVG(error_rate) as avg_error_rate
FROM lanestatusshard
GROUP BY region
ORDER BY region;
