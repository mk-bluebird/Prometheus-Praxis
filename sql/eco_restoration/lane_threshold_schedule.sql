-- File: sql/eco_restoration/lane_threshold_schedule.sql

CREATE TABLE IF NOT EXISTS lane_threshold_schedule (
    schedule_id TEXT PRIMARY KEY,
    hour_start INTEGER NOT NULL CHECK(hour_start BETWEEN 0 AND 23),
    hour_end INTEGER NOT NULL CHECK(hour_end BETWEEN 0 AND 23),
    season TEXT NOT NULL CHECK(season IN ('monsoon','dry')),
    flow_min REAL NOT NULL,
    flow_max REAL NOT NULL CHECK(flow_max >= flow_min),
    k_min REAL NOT NULL CHECK(k_min BETWEEN 0 AND 1),
    e_min REAL NOT NULL CHECK(e_min BETWEEN 0 AND 1),
    r_max REAL NOT NULL CHECK(r_max BETWEEN 0 AND 1),
    solar_k_gain REAL NOT NULL,
    solar_e_gain REAL NOT NULL,
    solar_r_gain REAL NOT NULL,
    flow_k_gain REAL NOT NULL,
    flow_e_gain REAL NOT NULL,
    flow_r_gain REAL NOT NULL
) STRICT;
