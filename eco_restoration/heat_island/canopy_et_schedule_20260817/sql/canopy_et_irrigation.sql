PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS phoenix_canopy_cells (
    canopy_cell_id INTEGER PRIMARY KEY,
    hex_anchor TEXT NOT NULL,
    surveyed_at_utc TEXT NOT NULL,
    pedestrian_surface_type TEXT NOT NULL CHECK (
        pedestrian_surface_type IN ('PAVEMENT', 'SIDEWALK', 'PLAZA', 'TRAIL', 'PARKING')
    ),
    area_m2 REAL NOT NULL CHECK (area_m2 > 0.0),
    pavement_temperature_c REAL NOT NULL,
    vegetation_surface_temperature_c REAL NOT NULL,
    target_surface_temperature_c REAL NOT NULL,
    existing_canopy_fraction REAL NOT NULL CHECK (
        existing_canopy_fraction BETWEEN 0.0 AND 1.0
    ),
    minimum_required_canopy_fraction REAL CHECK (
        minimum_required_canopy_fraction BETWEEN 0.0 AND 1.0
    ),
    canopy_status TEXT NOT NULL CHECK (
        canopy_status IN (
            'ALREADY_MET',
            'NO_COOLING_CONTRAST',
            'IMPOSSIBLE_WITH_DECLARED_VEGETATED_SURFACE_TEMPERATURE',
            'TARGET_MET_BY_EXISTING_CANOPY',
            'CANOPY_EXPANSION_REQUIRED'
        )
    ),
    ker_knowledge_factor REAL NOT NULL CHECK (ker_knowledge_factor BETWEEN 0.0 AND 1.0),
    ker_eco_impact_value REAL NOT NULL CHECK (ker_eco_impact_value BETWEEN 0.0 AND 1.0),
    ker_harm_risk REAL NOT NULL CHECK (ker_harm_risk BETWEEN 0.0 AND 1.0),
    ker_score REAL NOT NULL CHECK (ker_score BETWEEN -1.0 AND 1.0),
    CHECK (
        ABS(ker_score - ker_knowledge_factor * (ker_eco_impact_value - ker_harm_risk)) <= 0.000001
    )
);

CREATE TABLE IF NOT EXISTS climate_et_observations (
    climate_et_observation_id INTEGER PRIMARY KEY,
    hex_anchor TEXT NOT NULL,
    observed_date TEXT NOT NULL,
    et0_mm REAL NOT NULL CHECK (et0_mm >= 0.0 AND et0_mm <= 100.0),
    effective_precipitation_mm REAL NOT NULL CHECK (
        effective_precipitation_mm >= 0.0 AND effective_precipitation_mm <= 1000.0
    ),
    temperature_c REAL NOT NULL,
    relative_humidity_fraction REAL NOT NULL CHECK (
        relative_humidity_fraction BETWEEN 0.0 AND 1.0
    ),
    UNIQUE (hex_anchor, observed_date)
);

CREATE TABLE IF NOT EXISTS vegetation_irrigation_profiles (
    vegetation_irrigation_profile_id INTEGER PRIMARY KEY,
    profile_name TEXT NOT NULL UNIQUE,
    vegetation_class TEXT NOT NULL CHECK (
        vegetation_class IN ('NATIVE_TREE', 'NATIVE_SHRUB', 'NATIVE_GROUND_COVER', 'RESTORATION_SEEDLING')
    ),
    crop_coefficient_kc REAL NOT NULL CHECK (crop_coefficient_kc >= 0.0 AND crop_coefficient_kc <= 2.0),
    irrigated_area_m2 REAL NOT NULL CHECK (irrigated_area_m2 > 0.0),
    soil_moisture_deficit_fraction REAL NOT NULL CHECK (
        soil_moisture_deficit_fraction BETWEEN 0.0 AND 1.0
    ),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1))
);

CREATE TABLE IF NOT EXISTS canal_water_budget (
    canal_water_budget_id INTEGER PRIMARY KEY,
    hex_anchor TEXT NOT NULL,
    budget_date TEXT NOT NULL,
    allocated_irrigation_l REAL NOT NULL CHECK (allocated_irrigation_l >= 0.0),
    habitat_reserve_l REAL NOT NULL CHECK (habitat_reserve_l >= 0.0),
    conveyance_efficiency REAL NOT NULL CHECK (
        conveyance_efficiency BETWEEN 0.0 AND 1.0
    ),
    authorization_reference TEXT NOT NULL,
    CHECK (habitat_reserve_l <= allocated_irrigation_l),
    UNIQUE (hex_anchor, budget_date)
);

CREATE TABLE IF NOT EXISTS irrigation_schedule_assessments (
    irrigation_schedule_assessment_id INTEGER PRIMARY KEY,
    climate_et_observation_id INTEGER NOT NULL REFERENCES climate_et_observations(climate_et_observation_id),
    vegetation_irrigation_profile_id INTEGER NOT NULL REFERENCES vegetation_irrigation_profiles(vegetation_irrigation_profile_id),
    canal_water_budget_id INTEGER NOT NULL REFERENCES canal_water_budget(canal_water_budget_id),
    etc_mm REAL NOT NULL CHECK (etc_mm >= 0.0),
    plant_demand_l REAL NOT NULL CHECK (plant_demand_l >= 0.0),
    scheduled_irrigation_l REAL NOT NULL CHECK (scheduled_irrigation_l >= 0.0),
    unmet_demand_l REAL NOT NULL CHECK (unmet_demand_l >= 0.0),
    schedule_status TEXT NOT NULL CHECK (
        schedule_status IN (
            'WITHIN_DECLARED_WATER_BUDGET',
            'PARTIAL_IRRIGATION_WITH_UNMET_DEMAND',
            'HOLD_INSUFFICIENT_ALLOCATED_WATER'
        )
    ),
    ker_knowledge_factor REAL NOT NULL CHECK (ker_knowledge_factor BETWEEN 0.0 AND 1.0),
    ker_eco_impact_value REAL NOT NULL CHECK (ker_eco_impact_value BETWEEN 0.0 AND 1.0),
    ker_harm_risk REAL NOT NULL CHECK (ker_harm_risk BETWEEN 0.0 AND 1.0),
    ker_score REAL NOT NULL CHECK (ker_score BETWEEN -1.0 AND 1.0),
    CHECK (
        ABS(ker_score - ker_knowledge_factor * (ker_eco_impact_value - ker_harm_risk)) <= 0.000001
    ),
    UNIQUE (
        climate_et_observation_id,
        vegetation_irrigation_profile_id,
        canal_water_budget_id
    )
);

CREATE INDEX IF NOT EXISTS idx_canopy_hex_status
    ON phoenix_canopy_cells(hex_anchor, canopy_status, surveyed_at_utc DESC);

CREATE INDEX IF NOT EXISTS idx_climate_et_anchor_date
    ON climate_et_observations(hex_anchor, observed_date DESC, et0_mm);

CREATE INDEX IF NOT EXISTS idx_water_budget_anchor_date
    ON canal_water_budget(hex_anchor, budget_date DESC, allocated_irrigation_l);

CREATE INDEX IF NOT EXISTS idx_irrigation_schedule_status
    ON irrigation_schedule_assessments(schedule_status, unmet_demand_l DESC, etc_mm);

CREATE TRIGGER IF NOT EXISTS trg_irrigation_budget_consistency
BEFORE INSERT ON irrigation_schedule_assessments
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.scheduled_irrigation_l > NEW.plant_demand_l
        THEN RAISE(ABORT, 'scheduled irrigation cannot exceed plant demand')
        WHEN NEW.scheduled_irrigation_l > (
            SELECT
                (allocated_irrigation_l - habitat_reserve_l) * conveyance_efficiency
            FROM canal_water_budget
            WHERE canal_water_budget_id = NEW.canal_water_budget_id
        )
        THEN RAISE(ABORT, 'scheduled irrigation exceeds available canal-water allocation')
        WHEN NEW.unmet_demand_l < NEW.plant_demand_l - NEW.scheduled_irrigation_l - 0.000001
        THEN RAISE(ABORT, 'unmet demand is inconsistent with demand and scheduled water')
    END;
END;

CREATE VIEW IF NOT EXISTS v_canopy_threshold_status AS
SELECT
    hex_anchor,
    pedestrian_surface_type,
    pavement_temperature_c,
    vegetation_surface_temperature_c,
    target_surface_temperature_c,
    existing_canopy_fraction,
    CASE
        WHEN target_surface_temperature_c >= pavement_temperature_c THEN 0.0
        WHEN vegetation_surface_temperature_c >= pavement_temperature_c THEN NULL
        WHEN (
            pavement_temperature_c - target_surface_temperature_c
        ) / (
            pavement_temperature_c - vegetation_surface_temperature_c
        ) > 1.0 THEN NULL
        ELSE (
            pavement_temperature_c - target_surface_temperature_c
        ) / (
            pavement_temperature_c - vegetation_surface_temperature_c
        )
    END AS modeled_required_canopy_fraction,
    canopy_status,
    ker_knowledge_factor,
    ker_eco_impact_value,
    ker_harm_risk,
    ker_score
FROM phoenix_canopy_cells;

CREATE VIEW IF NOT EXISTS v_et_limited_irrigation_schedule AS
SELECT
    climate.hex_anchor,
    climate.observed_date,
    profile.profile_name,
    profile.vegetation_class,
    climate.et0_mm,
    profile.crop_coefficient_kc,
    climate.et0_mm * profile.crop_coefficient_kc AS etc_mm,
    profile.irrigated_area_m2,
    profile.soil_moisture_deficit_fraction,
    (climate.et0_mm * profile.crop_coefficient_kc)
        * profile.irrigated_area_m2
        * profile.soil_moisture_deficit_fraction AS deficit_adjusted_demand_l,
    (budget.allocated_irrigation_l - budget.habitat_reserve_l)
        * budget.conveyance_efficiency AS usable_canal_supply_l,
    MIN(
        (climate.et0_mm * profile.crop_coefficient_kc)
            * profile.irrigated_area_m2
            * profile.soil_moisture_deficit_fraction,
        (budget.allocated_irrigation_l - budget.habitat_reserve_l)
            * budget.conveyance_efficiency
    ) AS scheduled_irrigation_l
FROM climate_et_observations AS climate
JOIN vegetation_irrigation_profiles AS profile
    ON profile.active = 1
JOIN canal_water_budget AS budget
    ON budget.hex_anchor = climate.hex_anchor
   AND budget.budget_date = climate.observed_date;
