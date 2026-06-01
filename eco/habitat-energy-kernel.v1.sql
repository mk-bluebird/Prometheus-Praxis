CREATE TABLE IF NOT EXISTS habitat_energy_kernel (
    site_id                     TEXT PRIMARY KEY,
    jurisdiction_id             TEXT NOT NULL,
    uhi_factor_c_per_mw         REAL NOT NULL,
    water_stress_index          REAL NOT NULL CHECK (water_stress_index >= 0.0 AND water_stress_index <= 1.0),
    grid_carbon_intensity_g_per_kwh REAL NOT NULL,
    kernel_source               TEXT NOT NULL,
    confidence_score            REAL NOT NULL CHECK (confidence_score >= 0.0 AND confidence_score <= 1.0),
    FOREIGN KEY (jurisdiction_id) REFERENCES jurisdictions(jurisdiction_id)
);
