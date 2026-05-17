-- filename: db/v_econet_stake_karma_daily_phx.sql
-- destination: eco_restoration_shard/db/v_econet_stake_karma_daily_phx.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- Daily eco-wealth surface for Phoenix, derived from stake terminal batch.
-- Source table: econet_stake_terminal_batch_2026q2_phx
-- Assumes columns:
--   stewarddid TEXT
--   windowendutc TEXT (ISO8601)
--   ecounitfinal REAL
--   lane TEXT
--   region TEXT
-------------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_econet_stake_karma_daily_phx AS
SELECT
    date(windowendutc)                AS day_utc,
    region                            AS region,
    lane                              AS lane,
    stewarddid                        AS stewarddid,
    COUNT(*)                          AS shard_count,
    SUM(ecounitfinal)                 AS ecounitfinal_sum,
    AVG(ecounitfinal)                 AS ecounitfinal_avg,
    MIN(ecounitfinal)                 AS ecounitfinal_min,
    MAX(ecounitfinal)                 AS ecounitfinal_max
FROM econet_stake_terminal_batch_2026q2_phx
WHERE region = 'Phoenix-AZ-US'
GROUP BY
    day_utc,
    region,
    lane,
    stewarddid;

-------------------------------------------------------------------------------
-- Optional materialized cache table for AI-chat / T03 surfaces.
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS daily_eco_surface_phx (
    day_utc             TEXT    NOT NULL,
    region              TEXT    NOT NULL,
    lane                TEXT    NOT NULL,
    stewarddid          TEXT    NOT NULL,
    shard_count         INTEGER NOT NULL,
    ecounitfinal_sum    REAL    NOT NULL,
    ecounitfinal_avg    REAL    NOT NULL,
    ecounitfinal_min    REAL    NOT NULL,
    ecounitfinal_max    REAL    NOT NULL,
    PRIMARY KEY (day_utc, region, lane, stewarddid)
);

CREATE INDEX IF NOT EXISTS idx_daily_eco_surface_phx_region_day
    ON daily_eco_surface_phx (region, day_utc);

-- A scheduled job can periodically sync:
-- INSERT OR REPLACE INTO daily_eco_surface_phx
-- SELECT * FROM v_econet_stake_karma_daily_phx;
