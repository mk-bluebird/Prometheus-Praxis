PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS canal_nodes (
    canal_node_id TEXT PRIMARY KEY NOT NULL,
    node_name TEXT NOT NULL UNIQUE,
    fog_media_state TEXT NOT NULL CHECK (
        fog_media_state IN (
            'AQUEOUS_MODELED',
            'FIELD_SCREENED_NOT_ALL_FOG_CONDITIONS_MET',
            'UNMODELED_MIXED_OIL_WATER_SEDIMENT',
            'UNKNOWN_MEDIA'
        )
    ),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1))
);

CREATE TABLE IF NOT EXISTS ker_assessments (
    ker_assessment_id INTEGER PRIMARY KEY,
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor BETWEEN 0.0 AND 1.0),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value BETWEEN 0.0 AND 1.0),
    harm_risk REAL NOT NULL CHECK (harm_risk BETWEEN 0.0 AND 1.0),
    ker_score REAL NOT NULL CHECK (ker_score BETWEEN -1.0 AND 1.0),
    CHECK (ABS(ker_score - knowledge_factor * (eco_impact_value - harm_risk)) <= 0.000001)
);

CREATE TABLE IF NOT EXISTS vegetated_swale_designs (
    swale_design_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    design_name TEXT NOT NULL UNIQUE,
    length_m REAL NOT NULL CHECK (length_m > 0.0 AND length_m <= 10000.0),
    slope_fraction REAL NOT NULL CHECK (slope_fraction >= 0.0 AND slope_fraction <= 1.0),
    vegetation_density REAL NOT NULL CHECK (vegetation_density BETWEEN 0.0 AND 1.0),
    hydraulic_loading_m_per_h REAL NOT NULL CHECK (
        hydraulic_loading_m_per_h >= 0.0 AND hydraulic_loading_m_per_h <= 10000.0
    ),
    check_dam_present INTEGER NOT NULL CHECK (check_dam_present IN (0, 1)),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id)
);

CREATE TABLE IF NOT EXISTS swale_tss_observations (
    swale_tss_observation_id INTEGER PRIMARY KEY,
    swale_design_id INTEGER NOT NULL REFERENCES vegetated_swale_designs(swale_design_id) ON DELETE CASCADE,
    observed_at_utc TEXT NOT NULL,
    influent_tss_mg_l REAL NOT NULL CHECK (influent_tss_mg_l > 0.0),
    effluent_tss_mg_l REAL NOT NULL CHECK (
        effluent_tss_mg_l >= 0.0 AND effluent_tss_mg_l <= influent_tss_mg_l
    ),
    mean_velocity_m_per_h REAL NOT NULL CHECK (mean_velocity_m_per_h > 0.0),
    hydraulic_loading_m_per_h REAL NOT NULL CHECK (
        hydraulic_loading_m_per_h >= 0.0 AND hydraulic_loading_m_per_h <= 10000.0
    ),
    removal_fraction REAL GENERATED ALWAYS AS (
        1.0 - effluent_tss_mg_l / influent_tss_mg_l
    ) STORED,
    UNIQUE (swale_design_id, observed_at_utc)
);

CREATE TABLE IF NOT EXISTS stormwater_media (
    stormwater_media_id INTEGER PRIMARY KEY,
    media_name TEXT NOT NULL UNIQUE,
    media_type TEXT NOT NULL CHECK (media_type IN ('BIOCHAR', 'SAND', 'BIOCHAR_SAND_BLEND')),
    biochar_mass_fraction REAL NOT NULL CHECK (biochar_mass_fraction BETWEEN 0.0 AND 1.0),
    cec_cmolc_per_kg REAL NOT NULL CHECK (cec_cmolc_per_kg >= 0.0 AND cec_cmolc_per_kg <= 10000.0),
    bulk_density_kg_per_m3 REAL NOT NULL CHECK (bulk_density_kg_per_m3 > 0.0),
    grain_size_mm REAL NOT NULL CHECK (grain_size_mm > 0.0),
    source_description TEXT NOT NULL,
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id)
);

CREATE TABLE IF NOT EXISTS langmuir_sorption_experiments (
    langmuir_experiment_id INTEGER PRIMARY KEY,
    stormwater_media_id INTEGER NOT NULL REFERENCES stormwater_media(stormwater_media_id),
    analyte_name TEXT NOT NULL,
    temperature_c REAL NOT NULL CHECK (temperature_c BETWEEN -5.0 AND 60.0),
    ph REAL NOT NULL CHECK (ph BETWEEN 0.0 AND 14.0),
    contact_time_h REAL NOT NULL CHECK (contact_time_h > 0.0 AND contact_time_h <= 8760.0),
    equilibrium_concentration_mg_l REAL NOT NULL CHECK (equilibrium_concentration_mg_l >= 0.0),
    sorbed_loading_mg_per_g REAL NOT NULL CHECK (sorbed_loading_mg_per_g >= 0.0),
    replicate_id INTEGER NOT NULL CHECK (replicate_id BETWEEN 1 AND 100),
    UNIQUE (
        stormwater_media_id,
        analyte_name,
        temperature_c,
        ph,
        contact_time_h,
        equilibrium_concentration_mg_l,
        replicate_id
    )
);

CREATE TABLE IF NOT EXISTS langmuir_fit_results (
    langmuir_fit_result_id INTEGER PRIMARY KEY,
    stormwater_media_id INTEGER NOT NULL REFERENCES stormwater_media(stormwater_media_id),
    analyte_name TEXT NOT NULL,
    fitted_qmax_mg_per_g REAL NOT NULL CHECK (fitted_qmax_mg_per_g > 0.0),
    fitted_k_l_per_mg REAL NOT NULL CHECK (fitted_k_l_per_mg > 0.0),
    sum_squared_error REAL NOT NULL CHECK (sum_squared_error >= 0.0),
    temperature_c REAL NOT NULL CHECK (temperature_c BETWEEN -5.0 AND 60.0),
    ph REAL NOT NULL CHECK (ph BETWEEN 0.0 AND 14.0),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id),
    UNIQUE (stormwater_media_id, analyte_name, temperature_c, ph)
);

CREATE INDEX IF NOT EXISTS idx_swale_observations_design_time
    ON swale_tss_observations(swale_design_id, observed_at_utc DESC, removal_fraction);

CREATE INDEX IF NOT EXISTS idx_swale_design_slope_vegetation_loading
    ON vegetated_swale_designs(slope_fraction, vegetation_density, hydraulic_loading_m_per_h);

CREATE INDEX IF NOT EXISTS idx_langmuir_experiment_media_analyte
    ON langmuir_sorption_experiments(
        stormwater_media_id,
        analyte_name,
        temperature_c,
        ph,
        equilibrium_concentration_mg_l
    );

CREATE INDEX IF NOT EXISTS idx_langmuir_fit_media_analyte
    ON langmuir_fit_results(stormwater_media_id, analyte_name, temperature_c, ph);

CREATE TRIGGER IF NOT EXISTS trg_biochar_fraction_matches_type
BEFORE INSERT ON stormwater_media
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.media_type = 'BIOCHAR' AND NEW.biochar_mass_fraction <> 1.0
        THEN RAISE(ABORT, 'BIOCHAR media must have biochar mass fraction of 1')
        WHEN NEW.media_type = 'SAND' AND NEW.biochar_mass_fraction <> 0.0
        THEN RAISE(ABORT, 'SAND media must have biochar mass fraction of 0')
        WHEN NEW.media_type = 'BIOCHAR_SAND_BLEND'
         AND (NEW.biochar_mass_fraction <= 0.0 OR NEW.biochar_mass_fraction >= 1.0)
        THEN RAISE(ABORT, 'BIOCHAR_SAND_BLEND must have biochar mass fraction between 0 and 1')
    END;
END;

CREATE VIEW IF NOT EXISTS v_swale_tss_performance AS
SELECT
    design.design_name,
    design.length_m,
    design.slope_fraction,
    design.vegetation_density,
    design.check_dam_present,
    observation.observed_at_utc,
    observation.influent_tss_mg_l,
    observation.effluent_tss_mg_l,
    observation.mean_velocity_m_per_h,
    observation.removal_fraction,
    ker.knowledge_factor,
    ker.eco_impact_value,
    ker.harm_risk,
    ker.ker_score
FROM swale_tss_observations AS observation
JOIN vegetated_swale_designs AS design ON design.swale_design_id = observation.swale_design_id
JOIN ker_assessments AS ker ON ker.ker_assessment_id = design.ker_assessment_id;

CREATE VIEW IF NOT EXISTS v_langmuir_media_capacity AS
SELECT
    media.media_name,
    media.media_type,
    media.biochar_mass_fraction,
    media.cec_cmolc_per_kg,
    experiment.analyte_name,
    experiment.equilibrium_concentration_mg_l,
    experiment.sorbed_loading_mg_per_g,
    fit.fitted_qmax_mg_per_g,
    fit.fitted_k_l_per_mg,
    fit.sum_squared_error,
    fit.temperature_c,
    fit.ph
FROM langmuir_sorption_experiments AS experiment
JOIN stormwater_media AS media ON media.stormwater_media_id = experiment.stormwater_media_id
LEFT JOIN langmuir_fit_results AS fit
    ON fit.stormwater_media_id = experiment.stormwater_media_id
   AND fit.analyte_name = experiment.analyte_name
   AND fit.temperature_c = experiment.temperature_c
   AND fit.ph = experiment.ph;
