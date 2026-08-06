-- File: sql/cyboquatic/workload_telemetry_2026_08_04.sql

-- This SQL shard defines the telemetry schema and indices for cyboquatic workload logging.
-- It is designed for SQLite and emphasizes eco-restoration KER invariants:
--   K (Knowledge): data about machinery workloads and eco-scores
--   E (Energy): explicit energyreqJ metrics
--   R (Restoration): eco_intensity and eco_score focusing on ecological benefit

PRAGMA foreign_keys = ON;

-- Canonical table capturing machines and their eco-restoration roles.
CREATE TABLE IF NOT EXISTS cybo_machine (
    machine_id TEXT PRIMARY KEY,
    description TEXT NOT NULL,
    role TEXT NOT NULL,
    location_id TEXT NOT NULL,
    eco_role_label TEXT NOT NULL, -- e.g., "sediment_cleaner", "wetland_aerator"
    k_tag TEXT NOT NULL,          -- knowledge tag for analytics
    e_tag TEXT NOT NULL,          -- energy tag (e.g., "low_energy", "medium_energy")
    r_tag TEXT NOT NULL,          -- restoration tag (e.g., "high_restoration")
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Workload telemetry table.
CREATE TABLE IF NOT EXISTS cybo_workload_telemetry (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp_iso8601 TEXT NOT NULL,
    machine_id TEXT NOT NULL,
    location_id TEXT NOT NULL,
    energyreqJ REAL NOT NULL CHECK (energyreqJ > 0.0),
    deltaVt REAL NOT NULL CHECK (deltaVt > 0.0),
    eco_intensity REAL NOT NULL CHECK (eco_intensity >= 0.0 AND eco_intensity <= 1.0),
    eco_score REAL NOT NULL CHECK (eco_score >= 0.0 AND eco_score <= 1.0),
    canal_node TEXT NOT NULL,      -- canal node identifier within cyboquatic network
    fog_node TEXT NOT NULL,        -- FOG-router node identifier
    ker_state TEXT NOT NULL,       -- encoded K/E/R triad state (e.g., "K:stable;E:low;R:high")
    FOREIGN KEY (machine_id) REFERENCES cybo_machine(machine_id) ON DELETE CASCADE
);

-- Strict invariants:
--  - eco_score must be at least as good as (1 - eco_intensity)/2 to discourage high energy intensity.
--  - energyreqJ must be within a safe upper bound for the specific machine_id.
-- Implement via trigger to keep SQLite-compatible and portable.

CREATE TABLE IF NOT EXISTS cybo_machine_energy_bounds (
    machine_id TEXT PRIMARY KEY,
    energyreqJ_max REAL NOT NULL CHECK (energyreqJ_max > 0.0),
    FOREIGN KEY (machine_id) REFERENCES cybo_machine(machine_id) ON DELETE CASCADE
);

-- Trigger to enforce eco_score lower bound relative to eco_intensity.
CREATE TRIGGER IF NOT EXISTS trg_cybo_workload_ker_invariant
BEFORE INSERT ON cybo_workload_telemetry
FOR EACH ROW
BEGIN
    -- eco_score must not fall below half of the ideal score.
    -- ideal_score = (1.0 - eco_intensity); invariant: eco_score >= ideal_score / 2.
    SELECT
        CASE
            WHEN NEW.eco_score < ((1.0 - NEW.eco_intensity) / 2.0)
            THEN RAISE(ABORT, 'KER invariant violation: eco_score too low for given eco_intensity')
        END;
END;

-- Trigger to enforce machine-specific energy upper bound.
CREATE TRIGGER IF NOT EXISTS trg_cybo_workload_energy_bound
BEFORE INSERT ON cybo_workload_telemetry
FOR EACH ROW
BEGIN
    SELECT
        CASE
            WHEN EXISTS (
                SELECT 1
                FROM cybo_machine_energy_bounds b
                WHERE b.machine_id = NEW.machine_id
                  AND NEW.energyreqJ > b.energyreqJ_max
            )
            THEN RAISE(ABORT, 'Energy bound violation: energyreqJ exceeds configured maximum for machine')
        END;
END;

-- Indices optimized for eco-restoration analysis and blast-radius investigations.
CREATE INDEX IF NOT EXISTS idx_workload_machine_time
    ON cybo_workload_telemetry (machine_id, timestamp_iso8601);

CREATE INDEX IF NOT EXISTS idx_workload_eco_score
    ON cybo_workload_telemetry (eco_score);

CREATE INDEX IF NOT EXISTS idx_workload_canal_fog
    ON cybo_workload_telemetry (canal_node, fog_node);

-- Example seed data for machines and energy bounds, aligned with the C++ simulator.

INSERT INTO cybo_machine (machine_id, description, role, location_id,
                          eco_role_label, k_tag, e_tag, r_tag)
VALUES
    ('machine_sediment_cleaner_v1',
     'Primary sediment cleaner for canal sector alpha',
     'sediment_cleaning',
     'canal_sector_alpha',
     'sediment_cleaner',
     'K:sediment_profile',
     'E:low',
     'R:high'),
    ('machine_wetland_aerator_v2',
     'Aeration unit for wetland beta',
     'wetland_aeration',
     'wetland_beta',
     'wetland_aerator',
     'K:wetland_oxygen',
     'E:medium',
     'R:high'),
    ('machine_pfas_filter_v1',
     'PFAS filtration unit for treatment gamma',
     'pfas_filtration',
     'treatment_gamma',
     'pfas_filter',
     'K:pfas_fate',
     'E:medium',
     'R:critical');

INSERT INTO cybo_machine_energy_bounds (machine_id, energyreqJ_max)
VALUES
    ('machine_sediment_cleaner_v1', 8000.0),
    ('machine_wetland_aerator_v2', 12000.0),
    ('machine_pfas_filter_v1', 16000.0);
