-- File: sql/surcharge_breach_index_strategy.sql
PRAGMA foreign_keys = ON;

-- Blast-radius surcharge table schema (simplified for index design).
CREATE TABLE IF NOT EXISTS blast_radius_surcharge (
    event_id INTEGER PRIMARY KEY AUTOINCREMENT,
    canal_segment TEXT NOT NULL,
    upstream_level_m REAL NOT NULL,
    soil_moisture_class INTEGER NOT NULL,  -- e.g., 0=dry,1=moderate,2=wet
    forecast_surge_2h_m REAL NOT NULL,     -- 2-hour forecast surge level
    breach_probability REAL NOT NULL,      -- 0..1
    flood_polygon_id TEXT NOT NULL
);

-- Covering index strategy:
-- We want to support queries of the form:
--   "find all segments with >80% probability of breach given a 2-hour forecast surge"
--   filtered by canal_segment, upstream_level, soil_moisture_class.
--
-- A covering index that includes all columns needed for such queries allows
-- SQLite to use an index-only scan: the query can be answered from the index
-- without touching the table.
--
-- Define composite index:
CREATE INDEX IF NOT EXISTS idx_surcharge_cover
    ON blast_radius_surcharge (
        canal_segment,
        upstream_level_m,
        soil_moisture_class,
        breach_probability,
        forecast_surge_2h_m,
        flood_polygon_id
    );

-- Example query:
--   SELECT canal_segment, upstream_level_m, soil_moisture_class, breach_probability, flood_polygon_id
--   FROM blast_radius_surcharge
--   WHERE forecast_surge_2h_m >= :surge_threshold
--     AND breach_probability > 0.8;

-- EXPLAIN QUERY PLAN for the above query should show index-only scan usage.

-- Example EXPLAIN QUERY PLAN (conceptual; actual output produced by SQLite on execution):
-- EXPLAIN QUERY PLAN
-- SELECT canal_segment, upstream_level_m, soil_moisture_class, breach_probability, flood_polygon_id
-- FROM blast_radius_surcharge
-- WHERE forecast_surge_2h_m >= 1.5
--   AND breach_probability > 0.8;

-- Expected plan (simplified):
--   QUERY PLAN
--   `SEARCH blast_radius_surcharge USING INDEX idx_surcharge_cover
--      (forecast_surge_2h_m>? AND breach_probability>?)`
--   This indicates that SQLite searches using idx_surcharge_cover and, because the
--   selected columns are all included in the index, the scan can be index-only.
