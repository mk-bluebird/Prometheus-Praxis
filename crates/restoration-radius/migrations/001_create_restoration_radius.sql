CREATE TABLE restoration_radius (
    radius_id UUID PRIMARY KEY,
    center_node_id UUID NOT NULL,
    radius_km DOUBLE PRECISION NOT NULL,
    pollutant_mass_removed_kg DOUBLE PRECISION NOT NULL,
    karmadelta DOUBLE PRECISION NOT NULL,
    r_gw DOUBLE PRECISION NOT NULL,
    r_gw_ci95 DOUBLE PRECISION NOT NULL,
    calibration_score DOUBLE PRECISION NOT NULL,
    computed_at TIMESTAMPTZ NOT NULL,
    basin_id TEXT NOT NULL,
    geom GEOMETRY(POLYGON, 4326) NOT NULL
);

CREATE INDEX idx_restoration_radius_basin
ON restoration_radius(basin_id);

CREATE INDEX idx_restoration_radius_geom
ON restoration_radius
USING GIST (geom);
