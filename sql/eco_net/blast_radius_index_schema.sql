-- File: sql/eco_net/blast_radius_index_schema.sql

PRAGMA foreign_keys = ON;

-- Blast-radius scenario table:
-- Stores high-level scenario parameters keyed by scenario_id,
-- hex_id, canal_node, and surcharge level.[59]

CREATE TABLE IF NOT EXISTS blast_radius_scenario (
    scenario_id INTEGER PRIMARY KEY AUTOINCREMENT,
    hex_id TEXT NOT NULL,
    canal_node TEXT NOT NULL,
    surcharge_level REAL NOT NULL CHECK (surcharge_level >= 0.0),
    description TEXT NOT NULL,
    lane TEXT NOT NULL CHECK (lane IN ('RESEARCH', 'EXPPROD', 'PROD')),
    owner_did TEXT NOT NULL,
    createdutc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updatedutc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

-- Blast-radius result table:
-- Stores outputs from C++ Phoenix canal blast-radius simulations
-- indexed by scenario and hex.[59]

CREATE TABLE IF NOT EXISTS blast_radius_result (
    result_id INTEGER PRIMARY KEY AUTOINCREMENT,
    scenario_id INTEGER NOT NULL REFERENCES blast_radius_scenario(scenario_id) ON DELETE CASCADE,
    hex_id TEXT NOT NULL,
    canal_node TEXT NOT NULL,
    timestep INTEGER NOT NULL CHECK (timestep >= 0),
    r_hydraulics REAL NOT NULL CHECK (r_hydraulics >= 0.0 AND r_hydraulics <= 1.0),
    r_energy REAL NOT NULL CHECK (r_energy >= 0.0 AND r_energy <= 1.0),
    r_topology REAL NOT NULL CHECK (r_topology >= 0.0 AND r_topology <= 1.0),
    Vt REAL NOT NULL CHECK (Vt >= 0.0),
    surcharge_level REAL NOT NULL CHECK (surcharge_level >= 0.0),
    createdutc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

-- Indices for efficient querying by hex, canal node, and surcharge level.

CREATE INDEX IF NOT EXISTS idx_blast_radius_scenario_hex_node
    ON blast_radius_scenario (hex_id, canal_node);

CREATE INDEX IF NOT EXISTS idx_blast_radius_scenario_surcharge
    ON blast_radius_scenario (surcharge_level);

CREATE INDEX IF NOT EXISTS idx_blast_radius_result_hex_node
    ON blast_radius_result (hex_id, canal_node);

CREATE INDEX IF NOT EXISTS idx_blast_radius_result_scenario_timestep
    ON blast_radius_result (scenario_id, timestep);

CREATE INDEX IF NOT EXISTS idx_blast_radius_result_surcharge
    ON blast_radius_result (surcharge_level);

-- Optional helper view linking scenarios and results for AI agents.

CREATE VIEW IF NOT EXISTS v_blast_radius_scenarios AS
SELECT
    s.scenario_id,
    s.hex_id,
    s.canal_node,
    s.surcharge_level,
    s.description,
    s.lane,
    s.owner_did,
    r.result_id,
    r.timestep,
    r.r_hydraulics,
    r.r_energy,
    r.r_topology,
    r.Vt
FROM blast_radius_scenario s
LEFT JOIN blast_radius_result r
    ON r.scenario_id = s.scenario_id
ORDER BY s.scenario_id, r.timestep;
