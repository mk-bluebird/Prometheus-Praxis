PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS phoenix_land_cover_cells (
    land_cover_cell_id INTEGER PRIMARY KEY,
    hex_anchor TEXT NOT NULL,
    surveyed_at_utc TEXT NOT NULL,
    surface_treatment TEXT NOT NULL CHECK (
        surface_treatment IN ('HIGH_ALBEDO', 'NATIVE_VEGETATION', 'SHADE_STRUCTURE', 'WATER_FEATURE', 'COMBINED')
    ),
    area_m2 REAL NOT NULL CHECK (area_m2 > 0.0),
    baseline_albedo REAL NOT NULL CHECK (baseline_albedo BETWEEN 0.0 AND 1.0),
    treatment_albedo REAL NOT NULL CHECK (treatment_albedo BETWEEN 0.0 AND 1.0),
    net_radiation_w_m2 REAL NOT NULL,
    latent_heat_flux_w_m2 REAL NOT NULL CHECK (latent_heat_flux_w_m2 >= 0.0),
    heat_transfer_coefficient_w_m2_k REAL NOT NULL CHECK (heat_transfer_coefficient_w_m2_k > 0.0),
    lifecycle_cost REAL NOT NULL CHECK (lifecycle_cost > 0.0),
    water_suitability REAL NOT NULL CHECK (water_suitability BETWEEN 0.0 AND 1.0),
    habitat_suitability REAL NOT NULL CHECK (habitat_suitability BETWEEN 0.0 AND 1.0),
    ker_knowledge_factor REAL NOT NULL CHECK (ker_knowledge_factor BETWEEN 0.0 AND 1.0),
    ker_eco_impact_value REAL NOT NULL CHECK (ker_eco_impact_value BETWEEN 0.0 AND 1.0),
    ker_harm_risk REAL NOT NULL CHECK (ker_harm_risk BETWEEN 0.0 AND 1.0),
    ker_score REAL NOT NULL CHECK (ker_score BETWEEN -1.0 AND 1.0),
    CHECK (
        ABS(ker_score - ker_knowledge_factor * (ker_eco_impact_value - ker_harm_risk)) <= 0.000001
    )
);

CREATE TABLE IF NOT EXISTS ai_compute_telemetry (
    ai_compute_telemetry_id INTEGER PRIMARY KEY,
    observed_at_utc TEXT NOT NULL,
    compute_source_id TEXT NOT NULL,
    it_power_w REAL NOT NULL CHECK (it_power_w >= 0.0),
    pue REAL NOT NULL CHECK (pue >= 1.0 AND pue <= 10.0),
    duration_s REAL NOT NULL CHECK (duration_s >= 0.0),
    recoverable_heat_fraction REAL NOT NULL CHECK (
        recoverable_heat_fraction BETWEEN 0.0 AND 1.0
    ),
    facility_energy_j REAL GENERATED ALWAYS AS (
        it_power_w * pue * duration_s
    ) STORED,
    recoverable_heat_j REAL GENERATED ALWAYS AS (
        it_power_w * pue * duration_s * recoverable_heat_fraction
    ) STORED,
    UNIQUE (compute_source_id, observed_at_utc)
);

CREATE TABLE IF NOT EXISTS eco_heat_sinks (
    eco_heat_sink_id INTEGER PRIMARY KEY,
    sink_name TEXT NOT NULL UNIQUE,
    sink_type TEXT NOT NULL CHECK (
        sink_type IN ('GREENHOUSE', 'SEED_GERMINATION', 'COMPOST', 'CANAL_WATER')
    ),
    heat_demand_j REAL NOT NULL CHECK (heat_demand_j >= 0.0),
    current_temperature_c REAL NOT NULL,
    maximum_temperature_c REAL NOT NULL,
    habitat_sensitivity REAL NOT NULL CHECK (habitat_sensitivity BETWEEN 0.0 AND 1.0),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    CHECK (maximum_temperature_c > current_temperature_c OR sink_type = 'CANAL_WATER')
);

CREATE TABLE IF NOT EXISTS heat_reuse_assessments (
    heat_reuse_assessment_id INTEGER PRIMARY KEY,
    ai_compute_telemetry_id INTEGER NOT NULL REFERENCES ai_compute_telemetry(ai_compute_telemetry_id),
    eco_heat_sink_id INTEGER NOT NULL REFERENCES eco_heat_sinks(eco_heat_sink_id),
    allocated_heat_j REAL NOT NULL CHECK (allocated_heat_j >= 0.0),
    assessment_status TEXT NOT NULL CHECK (
        assessment_status IN (
            'ELIGIBLE_FOR_SITE_THERMAL_REVIEW',
            'HOLD_TEMPERATURE_LIMIT',
            'REJECT_CANAL_WATER_WARMING',
            'REJECT_UNKNOWN_HEAT_SINK'
        )
    ),
    ker_knowledge_factor REAL NOT NULL CHECK (ker_knowledge_factor BETWEEN 0.0 AND 1.0),
    ker_eco_impact_value REAL NOT NULL CHECK (ker_eco_impact_value BETWEEN 0.0 AND 1.0),
    ker_harm_risk REAL NOT NULL CHECK (ker_harm_risk BETWEEN 0.0 AND 1.0),
    ker_score REAL NOT NULL CHECK (ker_score BETWEEN -1.0 AND 1.0),
    CHECK (
        ABS(ker_score - ker_knowledge_factor * (ker_eco_impact_value - ker_harm_risk)) <= 0.000001
    ),
    UNIQUE (ai_compute_telemetry_id, eco_heat_sink_id)
);

CREATE INDEX IF NOT EXISTS idx_land_cover_treatment_cost
    ON phoenix_land_cover_cells(surface_treatment, lifecycle_cost, hex_anchor);

CREATE INDEX IF NOT EXISTS idx_land_cover_suitability
    ON phoenix_land_cover_cells(water_suitability, habitat_suitability, surface_treatment);

CREATE INDEX IF NOT EXISTS idx_compute_telemetry_time
    ON ai_compute_telemetry(observed_at_utc DESC, recoverable_heat_j);

CREATE INDEX IF NOT EXISTS idx_heat_reuse_sink_status
    ON heat_reuse_assessments(eco_heat_sink_id, assessment_status, ai_compute_telemetry_id);

CREATE TRIGGER IF NOT EXISTS trg_heat_reuse_limits
BEFORE INSERT ON heat_reuse_assessments
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.allocated_heat_j > (
            SELECT recoverable_heat_j
            FROM ai_compute_telemetry
            WHERE ai_compute_telemetry_id = NEW.ai_compute_telemetry_id
        )
        THEN RAISE(ABORT, 'allocated heat exceeds recoverable heat telemetry')
        WHEN NEW.allocated_heat_j > (
            SELECT heat_demand_j
            FROM eco_heat_sinks
            WHERE eco_heat_sink_id = NEW.eco_heat_sink_id
        )
        THEN RAISE(ABORT, 'allocated heat exceeds sink heat demand')
        WHEN (
            SELECT sink_type
            FROM eco_heat_sinks
            WHERE eco_heat_sink_id = NEW.eco_heat_sink_id
        ) = 'CANAL_WATER'
        THEN RAISE(ABORT, 'automatic canal-water warming is prohibited')
        WHEN (
            SELECT current_temperature_c >= maximum_temperature_c
            FROM eco_heat_sinks
            WHERE eco_heat_sink_id = NEW.eco_heat_sink_id
        )
        THEN RAISE(ABORT, 'sink has reached its maximum temperature')
    END;
END;

CREATE VIEW IF NOT EXISTS v_surface_cooling_cost_effectiveness AS
SELECT
    hex_anchor,
    surface_treatment,
    area_m2,
    baseline_albedo,
    treatment_albedo,
    net_radiation_w_m2,
    latent_heat_flux_w_m2,
    heat_transfer_coefficient_w_m2_k,
    lifecycle_cost,
    (
        ((treatment_albedo - baseline_albedo) * net_radiation_w_m2)
        - latent_heat_flux_w_m2
    ) / heat_transfer_coefficient_w_m2_k AS delta_t_c,
    CASE
        WHEN (
            ((treatment_albedo - baseline_albedo) * net_radiation_w_m2)
            - latent_heat_flux_w_m2
        ) / heat_transfer_coefficient_w_m2_k < 0.0
        THEN -(
            ((treatment_albedo - baseline_albedo) * net_radiation_w_m2)
            - latent_heat_flux_w_m2
        ) / heat_transfer_coefficient_w_m2_k
        ) / lifecycle_cost
        ELSE 0.0
    END AS cooling_per_cost,
    ker_knowledge_factor,
    ker_eco_impact_value,
    ker_harm_risk,
    ker_score
FROM phoenix_land_cover_cells
WHERE water_suitability > 0.0
  AND habitat_suitability > 0.0;

CREATE VIEW IF NOT EXISTS v_eco_heat_reuse_status AS
SELECT
    telemetry.observed_at_utc,
    telemetry.compute_source_id,
    telemetry.it_power_w,
    telemetry.pue,
    telemetry.duration_s,
    telemetry.facility_energy_j,
    telemetry.recoverable_heat_j,
    sink.sink_name,
    sink.sink_type,
    sink.heat_demand_j,
    sink.current_temperature_c,
    sink.maximum_temperature_c,
    assessment.allocated_heat_j,
    assessment.assessment_status,
    assessment.ker_knowledge_factor,
    assessment.ker_eco_impact_value,
    assessment.ker_harm_risk,
    assessment.ker_score
FROM heat_reuse_assessments AS assessment
JOIN ai_compute_telemetry AS telemetry
    ON telemetry.ai_compute_telemetry_id = assessment.ai_compute_telemetry_id
JOIN eco_heat_sinks AS sink
    ON sink.eco_heat_sink_id = assessment.eco_heat_sink_id;
