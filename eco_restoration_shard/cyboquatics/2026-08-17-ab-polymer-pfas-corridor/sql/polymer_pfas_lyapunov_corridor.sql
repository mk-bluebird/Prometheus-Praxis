PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS fog_nodes (
    fog_node_id TEXT PRIMARY KEY NOT NULL,
    media_class TEXT NOT NULL CHECK (
        media_class IN ('AQUEOUS_SEDIMENT', 'BIOREACTOR', 'POLYMER_PROCESS', 'UNKNOWN_MEDIA')
    ),
    max_energy_j_per_observation REAL NOT NULL CHECK (
        max_energy_j_per_observation >= 0.0 AND max_energy_j_per_observation <= 1000000000.0
    ),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1))
);

CREATE TABLE IF NOT EXISTS canal_nodes (
    canal_node_id TEXT PRIMARY KEY NOT NULL,
    fog_node_id TEXT NOT NULL REFERENCES fog_nodes(fog_node_id),
    node_name TEXT NOT NULL UNIQUE,
    sediment_mass_kg REAL NOT NULL CHECK (sediment_mass_kg > 0.0),
    water_temperature_c REAL NOT NULL CHECK (water_temperature_c BETWEEN -10.0 AND 60.0),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1))
);

CREATE TABLE IF NOT EXISTS ker_profiles (
    ker_profile_id INTEGER PRIMARY KEY,
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor BETWEEN 0.0 AND 1.0),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value BETWEEN 0.0 AND 1.0),
    harm_risk REAL NOT NULL CHECK (harm_risk BETWEEN 0.0 AND 1.0),
    ker_score REAL NOT NULL CHECK (ker_score BETWEEN -1.0 AND 1.0),
    CHECK (ABS(ker_score - knowledge_factor * (eco_impact_value - harm_risk)) <= 0.000001)
);

CREATE TABLE IF NOT EXISTS polymer_material_parameters (
    polymer_material_id TEXT PRIMARY KEY NOT NULL,
    material_name TEXT NOT NULL UNIQUE,
    biodegradable_claim INTEGER NOT NULL CHECK (biodegradable_claim IN (0, 1)),
    k_hyd_per_day REAL NOT NULL CHECK (k_hyd_per_day >= 0.0 AND k_hyd_per_day <= 100.0),
    k_photo_per_day REAL NOT NULL CHECK (k_photo_per_day >= 0.0 AND k_photo_per_day <= 100.0),
    k_frag_per_day REAL NOT NULL CHECK (k_frag_per_day >= 0.0 AND k_frag_per_day <= 100.0),
    surface_erosion_per_day REAL NOT NULL CHECK (surface_erosion_per_day >= 0.0 AND surface_erosion_per_day <= 100.0),
    fragmentation_exponent REAL NOT NULL CHECK (fragmentation_exponent > 0.0 AND fragmentation_exponent <= 5.0),
    ker_profile_id INTEGER NOT NULL REFERENCES ker_profiles(ker_profile_id)
);

CREATE TABLE IF NOT EXISTS polymer_size_observations (
    polymer_observation_id INTEGER PRIMARY KEY,
    polymer_material_id TEXT NOT NULL REFERENCES polymer_material_parameters(polymer_material_id),
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    observed_at_utc TEXT NOT NULL,
    coarse_particle_count REAL NOT NULL CHECK (coarse_particle_count >= 0.0),
    fine_particle_count REAL NOT NULL CHECK (fine_particle_count >= 0.0),
    dissolved_carbon_mg_l REAL NOT NULL CHECK (dissolved_carbon_mg_l >= 0.0),
    observation_energy_j REAL NOT NULL CHECK (observation_energy_j >= 0.0),
    UNIQUE (polymer_material_id, canal_node_id, observed_at_utc)
);

CREATE TABLE IF NOT EXISTS pfas_chain_members (
    pfas_member_id INTEGER PRIMARY KEY,
    chain_id TEXT NOT NULL,
    sequence_index INTEGER NOT NULL CHECK (sequence_index >= 0),
    compound_label TEXT NOT NULL,
    first_order_rate_per_day REAL NOT NULL CHECK (
        first_order_rate_per_day >= 0.0 AND first_order_rate_per_day <= 100.0
    ),
    terminal_member INTEGER NOT NULL CHECK (terminal_member IN (0, 1)),
    UNIQUE (chain_id, sequence_index),
    UNIQUE (chain_id, compound_label)
);

CREATE TABLE IF NOT EXISTS pfas_state_observations (
    pfas_state_observation_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    pfas_member_id INTEGER NOT NULL REFERENCES pfas_chain_members(pfas_member_id),
    observed_at_utc TEXT NOT NULL,
    concentration_ng_kg REAL NOT NULL CHECK (concentration_ng_kg >= 0.0),
    measurement_energy_j REAL NOT NULL CHECK (measurement_energy_j >= 0.0),
    ker_profile_id INTEGER NOT NULL REFERENCES ker_profiles(ker_profile_id),
    UNIQUE (canal_node_id, pfas_member_id, observed_at_utc)
);

CREATE TABLE IF NOT EXISTS pfas_residual_corridor (
    residual_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    interval_index INTEGER NOT NULL CHECK (interval_index >= 0),
    observed_at_utc TEXT NOT NULL,
    state_dimension INTEGER NOT NULL CHECK (state_dimension >= 1 AND state_dimension <= 1024),
    residual_norm REAL NOT NULL CHECK (residual_norm >= 0.0),
    epsilon REAL NOT NULL CHECK (epsilon > 0.0),
    lambda_per_interval REAL NOT NULL CHECK (lambda_per_interval > 0.0 AND lambda_per_interval <= 100.0),
    corridor_limit REAL NOT NULL CHECK (corridor_limit >= 0.0),
    ker_profile_id INTEGER NOT NULL REFERENCES ker_profiles(ker_profile_id),
    UNIQUE (canal_node_id, interval_index),
    CHECK (
        ABS(corridor_limit - epsilon * EXP(-lambda_per_interval * interval_index)) <= 0.000001
    ),
    CHECK (residual_norm <= corridor_limit)
);

CREATE TABLE IF NOT EXISTS microbial_viability_corridor (
    viability_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    observed_at_utc TEXT NOT NULL,
    potential_v REAL NOT NULL CHECK (potential_v >= 0.0),
    previous_potential_v REAL NOT NULL CHECK (previous_potential_v >= 0.0),
    lambda_per_day REAL NOT NULL CHECK (lambda_per_day > 0.0 AND lambda_per_day <= 1.0),
    epsilon_per_day REAL NOT NULL CHECK (epsilon_per_day >= 0.0),
    thermal_residual REAL NOT NULL CHECK (thermal_residual >= 0.0),
    ker_profile_id INTEGER NOT NULL REFERENCES ker_profiles(ker_profile_id),
    CHECK (
        potential_v <= (1.0 - lambda_per_day) * previous_potential_v
            + epsilon_per_day + thermal_residual + 0.000001
    )
);

CREATE INDEX IF NOT EXISTS idx_polymer_observation_node_time
    ON polymer_size_observations(canal_node_id, observed_at_utc);

CREATE INDEX IF NOT EXISTS idx_pfas_observation_node_member_time
    ON pfas_state_observations(canal_node_id, pfas_member_id, observed_at_utc);

CREATE INDEX IF NOT EXISTS idx_pfas_residual_node_interval
    ON pfas_residual_corridor(canal_node_id, interval_index);

CREATE INDEX IF NOT EXISTS idx_viability_node_time
    ON microbial_viability_corridor(canal_node_id, observed_at_utc);

CREATE TRIGGER IF NOT EXISTS trg_polymer_observation_energy_limit
BEFORE INSERT ON polymer_size_observations
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.observation_energy_j > (
            SELECT fog.max_energy_j_per_observation
            FROM canal_nodes AS canal
            JOIN fog_nodes AS fog ON fog.fog_node_id = canal.fog_node_id
            WHERE canal.canal_node_id = NEW.canal_node_id
        )
        THEN RAISE(ABORT, 'polymer observation exceeds FOG energy limit')
    END;
END;

CREATE TRIGGER IF NOT EXISTS trg_pfas_terminal_sequence
BEFORE INSERT ON pfas_chain_members
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.terminal_member = 1 AND EXISTS (
            SELECT 1
            FROM pfas_chain_members
            WHERE chain_id = NEW.chain_id
              AND terminal_member = 1
        )
        THEN RAISE(ABORT, 'each PFAS chain may contain one terminal member')
    END;
END;

CREATE VIEW IF NOT EXISTS v_pfas_chain_clearance AS
SELECT
    member.chain_id,
    member.sequence_index,
    member.compound_label,
    member.first_order_rate_per_day,
    CASE
        WHEN member.first_order_rate_per_day > 0.0
        THEN LN(2.0) / member.first_order_rate_per_day
        ELSE NULL
    END AS half_life_days,
    member.terminal_member,
    CASE
        WHEN member.first_order_rate_per_day > 0.0 THEN 'DECAYING_STATE'
        ELSE 'PERSISTENT_STATE'
    END AS state_clearance_status
FROM pfas_chain_members AS member;

CREATE VIEW IF NOT EXISTS v_pfas_residual_compliance AS
SELECT
    residual.canal_node_id,
    residual.interval_index,
    residual.residual_norm,
    residual.corridor_limit,
    CASE
        WHEN residual.residual_norm <= residual.corridor_limit THEN 'COMPLIANT'
        ELSE 'NONCOMPLIANT'
    END AS status,
    ker.knowledge_factor,
    ker.eco_impact_value,
    ker.harm_risk,
    ker.ker_score
FROM pfas_residual_corridor AS residual
JOIN ker_profiles AS ker ON ker.ker_profile_id = residual.ker_profile_id;
