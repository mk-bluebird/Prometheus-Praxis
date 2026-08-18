PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS phoenix_hex_grid_definitions (
    hex_grid_definition_id INTEGER PRIMARY KEY,
    grid_version TEXT NOT NULL UNIQUE,
    origin_latitude_deg REAL NOT NULL CHECK (origin_latitude_deg BETWEEN -90.0 AND 90.0),
    origin_longitude_deg REAL NOT NULL CHECK (origin_longitude_deg BETWEEN -180.0 AND 180.0),
    meters_per_degree_latitude REAL NOT NULL CHECK (meters_per_degree_latitude > 0.0),
    meters_per_degree_longitude REAL NOT NULL CHECK (meters_per_degree_longitude > 0.0),
    hex_side_length_m REAL NOT NULL CHECK (hex_side_length_m > 0.0),
    projection_note TEXT NOT NULL,
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    created_at_utc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS phoenix_hex_anchors (
    phoenix_hex_anchor_id INTEGER PRIMARY KEY,
    hex_grid_definition_id INTEGER NOT NULL REFERENCES phoenix_hex_grid_definitions(hex_grid_definition_id),
    anchor_code TEXT NOT NULL UNIQUE,
    q INTEGER NOT NULL,
    r INTEGER NOT NULL,
    s INTEGER NOT NULL,
    center_x_m REAL NOT NULL,
    center_y_m REAL NOT NULL,
    center_latitude_deg REAL NOT NULL CHECK (center_latitude_deg BETWEEN -90.0 AND 90.0),
    center_longitude_deg REAL NOT NULL CHECK (center_longitude_deg BETWEEN -180.0 AND 180.0),
    created_at_utc TEXT NOT NULL,
    CHECK (s = -q - r),
    UNIQUE (hex_grid_definition_id, q, r, s)
);

CREATE TABLE IF NOT EXISTS canal_corridor_observations (
    canal_corridor_observation_id INTEGER PRIMARY KEY,
    observed_at_utc TEXT NOT NULL,
    source_id TEXT NOT NULL,
    metric_name TEXT NOT NULL CHECK (
        metric_name IN (
            'WATER_TEMPERATURE_C',
            'BOD_MG_L',
            'TSS_MG_L',
            'CONDUCTIVITY_US_CM',
            'HEAT_ISLAND_DELTA_C'
        )
    ),
    latitude_deg REAL NOT NULL CHECK (latitude_deg BETWEEN -90.0 AND 90.0),
    longitude_deg REAL NOT NULL CHECK (longitude_deg BETWEEN -180.0 AND 180.0),
    x_m REAL NOT NULL,
    y_m REAL NOT NULL,
    observed_value REAL NOT NULL,
    phoenix_hex_anchor_id INTEGER NOT NULL REFERENCES phoenix_hex_anchors(phoenix_hex_anchor_id),
    sensor_quality REAL NOT NULL CHECK (sensor_quality BETWEEN 0.0 AND 1.0),
    ker_knowledge_factor REAL NOT NULL CHECK (ker_knowledge_factor BETWEEN 0.0 AND 1.0),
    ker_eco_impact_value REAL NOT NULL CHECK (ker_eco_impact_value BETWEEN 0.0 AND 1.0),
    ker_harm_risk REAL NOT NULL CHECK (ker_harm_risk BETWEEN 0.0 AND 1.0),
    ker_score REAL NOT NULL CHECK (ker_score BETWEEN -1.0 AND 1.0),
    CHECK (
        ABS(ker_score - ker_knowledge_factor * (ker_eco_impact_value - ker_harm_risk)) <= 0.000001
    )
);

CREATE INDEX IF NOT EXISTS idx_hex_anchor_grid_qrs
    ON phoenix_hex_anchors(hex_grid_definition_id, q, r, s, phoenix_hex_anchor_id);

CREATE INDEX IF NOT EXISTS idx_corridor_hex_metric_time
    ON canal_corridor_observations(
        phoenix_hex_anchor_id,
        metric_name,
        observed_at_utc DESC,
        observed_value,
        sensor_quality
    );

CREATE INDEX IF NOT EXISTS idx_corridor_metric_hex_value
    ON canal_corridor_observations(
        metric_name,
        phoenix_hex_anchor_id,
        observed_value,
        observed_at_utc DESC
    );

CREATE VIEW IF NOT EXISTS v_hex_corridor_means AS
SELECT
    anchor.anchor_code,
    anchor.q,
    anchor.r,
    anchor.s,
    observation.metric_name,
    COUNT(*) AS sample_count,
    AVG(observation.observed_value) AS mean_value,
    MIN(observation.observed_value) AS minimum_value,
    MAX(observation.observed_value) AS maximum_value,
    AVG(observation.sensor_quality) AS mean_sensor_quality,
    AVG(observation.ker_knowledge_factor) AS mean_knowledge_factor,
    AVG(observation.ker_eco_impact_value) AS mean_eco_impact_value,
    AVG(observation.ker_harm_risk) AS mean_harm_risk
FROM canal_corridor_observations AS observation
JOIN phoenix_hex_anchors AS anchor
    ON anchor.phoenix_hex_anchor_id = observation.phoenix_hex_anchor_id
GROUP BY
    anchor.anchor_code,
    anchor.q,
    anchor.r,
    anchor.s,
    observation.metric_name;

CREATE VIEW IF NOT EXISTS v_hex_corridor_sparse_bins AS
SELECT
    anchor.anchor_code,
    observation.metric_name,
    COUNT(*) AS sample_count
FROM canal_corridor_observations AS observation
JOIN phoenix_hex_anchors AS anchor
    ON anchor.phoenix_hex_anchor_id = observation.phoenix_hex_anchor_id
GROUP BY anchor.anchor_code, observation.metric_name
HAVING COUNT(*) < 2;
