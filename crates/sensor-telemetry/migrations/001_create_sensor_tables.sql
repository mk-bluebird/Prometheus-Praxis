-- crates/sensor-telemetry/migrations/001_create_sensor_tables.sql

CREATE TABLE sensor_registry (
    sensor_id TEXT PRIMARY KEY,
    sensor_did TEXT NOT NULL,
    location GEOGRAPHY(POINT, 4326),
    installed_at TIMESTAMPTZ NOT NULL,
    meta JSONB NOT NULL DEFAULT '{}'
);

CREATE TABLE telemetry_particles (
    sensor_id TEXT NOT NULL,
    observed_at TIMESTAMPTZ NOT NULL,
    location GEOGRAPHY(POINT, 4326) NOT NULL,
    metrics JSONB NOT NULL,
    device_signature TEXT NOT NULL,
    PRIMARY KEY (sensor_id, observed_at)
);
SELECT create_hypertable('telemetry_particles', 'observed_at', if_not_exists => TRUE);

CREATE TABLE sensor_ker_adjustments (
    node_id TEXT NOT NULL,
    sensor_id TEXT NOT NULL,
    observed_at TIMESTAMPTZ NOT NULL,
    delta_k DOUBLE PRECISION NOT NULL,
    delta_e DOUBLE PRECISION NOT NULL,
    delta_r DOUBLE PRECISION NOT NULL,
    ker_before JSONB NOT NULL,
    ker_after JSONB NOT NULL,
    residual_before DOUBLE PRECISION NOT NULL,
    residual_after DOUBLE PRECISION NOT NULL,
    PRIMARY KEY (node_id, sensor_id, observed_at)
);

CREATE TABLE sensor_health_particles (
    sensor_id TEXT NOT NULL,
    checked_at TIMESTAMPTZ NOT NULL,
    reference_metric TEXT NOT NULL,
    deviation DOUBLE PRECISION NOT NULL,
    deviation_threshold DOUBLE PRECISION NOT NULL,
    healthy BOOLEAN NOT NULL,
    PRIMARY KEY (sensor_id, checked_at)
);
