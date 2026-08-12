-- File: sql/eco_restoration/hex_anchor_temporal_risk_views.sql
CREATE TABLE IF NOT EXISTS hex_environment_observation (
    observation_id INTEGER PRIMARY KEY,
    hex_anchor INTEGER NOT NULL,
    observed_unix_s INTEGER NOT NULL CHECK(observed_unix_s >= 0),
    heat_risk REAL NOT NULL CHECK(heat_risk BETWEEN 0.0 AND 1.0),
    water_risk REAL NOT NULL CHECK(water_risk BETWEEN 0.0 AND 1.0),
    biodiversity_risk REAL NOT NULL CHECK(biodiversity_risk BETWEEN 0.0 AND 1.0),
    quality_weight REAL NOT NULL DEFAULT 1.0 CHECK(quality_weight > 0.0 AND quality_weight <= 1.0),
    UNIQUE(hex_anchor, observed_unix_s)
) STRICT;

CREATE INDEX IF NOT EXISTS hex_environment_observation_anchor_time
ON hex_environment_observation(hex_anchor, observed_unix_s);

CREATE VIEW IF NOT EXISTS hex_anchor_daily_risk AS
SELECT
    hex_anchor,
    date(observed_unix_s, 'unixepoch') AS period_start_utc,
    COUNT(*) AS observation_count,
    SUM(heat_risk * quality_weight) / SUM(quality_weight) AS heat_risk,
    SUM(water_risk * quality_weight) / SUM(quality_weight) AS water_risk,
    SUM(biodiversity_risk * quality_weight) / SUM(quality_weight) AS biodiversity_risk,
    MAX(MAX(heat_risk, water_risk), biodiversity_risk) AS peak_risk
FROM hex_environment_observation
GROUP BY hex_anchor, date(observed_unix_s, 'unixepoch');

CREATE VIEW IF NOT EXISTS hex_anchor_weekly_risk AS
SELECT
    hex_anchor,
    strftime('%Y-%W', observed_unix_s, 'unixepoch') AS period_start_utc,
    COUNT(*) AS observation_count,
    SUM(heat_risk * quality_weight) / SUM(quality_weight) AS heat_risk,
    SUM(water_risk * quality_weight) / SUM(quality_weight) AS water_risk,
    SUM(biodiversity_risk * quality_weight) / SUM(quality_weight) AS biodiversity_risk,
    MAX(MAX(heat_risk, water_risk), biodiversity_risk) AS peak_risk
FROM hex_environment_observation
GROUP BY hex_anchor, strftime('%Y-%W', observed_unix_s, 'unixepoch');

CREATE VIEW IF NOT EXISTS hex_anchor_monthly_risk AS
SELECT
    hex_anchor,
    strftime('%Y-%m', observed_unix_s, 'unixepoch') AS period_start_utc,
    COUNT(*) AS observation_count,
    SUM(heat_risk * quality_weight) / SUM(quality_weight) AS heat_risk,
    SUM(water_risk * quality_weight) / SUM(quality_weight) AS water_risk,
    SUM(biodiversity_risk * quality_weight) / SUM(quality_weight) AS biodiversity_risk,
    MAX(MAX(heat_risk, water_risk), biodiversity_risk) AS peak_risk
FROM hex_environment_observation
GROUP BY hex_anchor, strftime('%Y-%m', observed_unix_s, 'unixepoch');
