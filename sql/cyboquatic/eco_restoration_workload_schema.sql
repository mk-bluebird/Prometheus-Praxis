-- File: sql/cyboquatic/eco_restoration_workload_schema.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS canal_node (
    node_id INTEGER PRIMARY KEY AUTOINCREMENT,
    node_code TEXT NOT NULL UNIQUE,
    description TEXT NOT NULL,
    ker_band TEXT NOT NULL CHECK (ker_band IN ('RESEARCH', 'EXPPROD', 'PROD')),
    fog_band TEXT NOT NULL CHECK (fog_band IN ('COLD_SURVIVAL_MONITOR', 'RESTORATION_PREFERRED', 'NEEDS_DIAGNOSTIC')),
    canal_plane TEXT NOT NULL CHECK (canal_plane IN ('HYDRAULICS', 'ENERGY', 'TOPOLOGY', 'BIODIVERSITY')),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1))
);

CREATE TABLE IF NOT EXISTS cyboquatic_workload_telemetry (
    sample_id INTEGER PRIMARY KEY AUTOINCREMENT,
    node_id INTEGER NOT NULL REFERENCES canal_node(node_id) ON DELETE CASCADE,
    timestamp_utc TEXT NOT NULL,
    energyreqJ REAL NOT NULL CHECK (energyreqJ >= 0.0),
    energy_input_J REAL NOT NULL CHECK (energy_input_J >= 0.0),
    deltaVt REAL NOT NULL CHECK (deltaVt >= 0.0 AND deltaVt <= 1.0),
    topo_stress_norm REAL NOT NULL CHECK (topo_stress_norm >= 0.0 AND topo_stress_norm <= 1.0),
    canal_temperature_C REAL NOT NULL,
    pfas_concentration_ugL REAL NOT NULL CHECK (pfas_concentration_ugL >= 0.0),
    fog_route TEXT NOT NULL CHECK (fog_route IN ('FOG:COLD_SURVIVAL_MONITOR','FOG:RESTORATION_PREFERRED','FOG:NEEDS_DIAGNOSTIC')),
    ker_hint TEXT NULL
);

CREATE TABLE IF NOT EXISTS canal_ker_canal_invariant (
    invariant_id INTEGER PRIMARY KEY AUTOINCREMENT,
    node_id INTEGER NOT NULL REFERENCES canal_node(node_id) ON DELETE CASCADE,
    k_value REAL NOT NULL CHECK (k_value >= 0.0 AND k_value <= 1.0),
    e_value REAL NOT NULL CHECK (e_value >= 0.0 AND e_value <= 1.0),
    r_value REAL NOT NULL CHECK (r_value >= 0.0 AND r_value <= 1.0),
    ker_score REAL NOT NULL CHECK (ker_score >= 0.0),
    createdutc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updatedutc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TRIGGER IF NOT EXISTS trg_canal_ker_canal_invariant_ker_update
AFTER INSERT ON canal_ker_canal_invariant
FOR EACH ROW
BEGIN
    UPDATE canal_ker_canal_invariant
    SET ker_score = (NEW.k_value * NEW.e_value - NEW.r_value),
        updatedutc = (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
    WHERE invariant_id = NEW.invariant_id;
END;

CREATE VIEW IF NOT EXISTS v_cyboquatic_workload_ker_summary AS
SELECT
    cn.node_code,
    AVG(ct.deltaVt) AS avg_deltaVt,
    AVG(ct.energy_input_J) AS avg_energy_input_J,
    AVG(ct.pfas_concentration_ugL) AS avg_pfas_ugL,
    MIN(ck.ker_score) AS min_ker_score
FROM canal_node cn
JOIN cyboquatic_workload_telemetry ct ON ct.node_id = cn.node_id
LEFT JOIN canal_ker_canal_invariant ck ON ck.node_id = cn.node_id
GROUP BY cn.node_code;

WITH RECURSIVE pfas_corridor(node_id, step, mass_kg, sorbed_fraction, cold_survival_factor) AS (
    SELECT
        node_id,
        0 AS step,
        1.0 AS mass_kg,
        0.5 AS sorbed_fraction,
        1.0 AS cold_survival_factor
    FROM canal_node
    WHERE active = 1
    UNION ALL
    SELECT
        p.node_id,
        p.step + 1,
        p.mass_kg * (1.0 - 0.01 * p.cold_survival_factor),
        p.sorbed_fraction + 0.001,
        CASE
            WHEN EXISTS (
                SELECT 1 FROM cyboquatic_workload_telemetry t
                WHERE t.node_id = p.node_id
                  AND t.canal_temperature_C <= 12.0
            )
            THEN p.cold_survival_factor * 1.02
            ELSE p.cold_survival_factor * 0.99
        END
    FROM pfas_corridor p
    WHERE p.step < 365
)
SELECT node_id, step, mass_kg, sorbed_fraction, cold_survival_factor
FROM pfas_corridor;
