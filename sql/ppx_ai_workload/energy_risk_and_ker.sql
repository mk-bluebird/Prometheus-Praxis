-- File: sql/ppx_ai_workload/energy_risk_and_ker.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS ppx_ai_energy_window (
    workload_id TEXT PRIMARY KEY,
    observed_utc TEXT NOT NULL,
    energy_j REAL NOT NULL CHECK(energy_j >= 0.0),
    renewable_fraction REAL NOT NULL CHECK(renewable_fraction BETWEEN 0.0 AND 1.0),
    marginal_carbon_g_per_kwh REAL NOT NULL CHECK(marginal_carbon_g_per_kwh >= 0.0),
    reference_carbon_g REAL NOT NULL CHECK(reference_carbon_g > 0.0),
    k_knowledge REAL NOT NULL CHECK(k_knowledge > 0.0 AND k_knowledge <= 1.0),
    ecological_value REAL NOT NULL CHECK(ecological_value > 0.0 AND ecological_value <= 1.0),
    r_energy REAL GENERATED ALWAYS AS (
        MIN(1.0, MAX(0.0,
            (energy_j / 3600000.0) * (1.0 - renewable_fraction) *
            marginal_carbon_g_per_kwh / reference_carbon_g
        ))
    ) STORED CHECK(r_energy BETWEEN 0.0 AND 1.0),
    e_multiplicative REAL GENERATED ALWAYS AS (
        k_knowledge * ecological_value * (1.0 - r_energy)
    ) STORED CHECK(e_multiplicative >= 0.0 AND e_multiplicative <= 1.0),
    e_additive_diagnostic REAL GENERATED ALWAYS AS (
        MIN(1.0, MAX(0.0,
            0.45 * k_knowledge + 0.45 * ecological_value - 0.10 * r_energy
        ))
    ) STORED CHECK(e_additive_diagnostic BETWEEN 0.0 AND 1.0)
) STRICT;

CREATE INDEX IF NOT EXISTS idx_ppx_energy_window_time
ON ppx_ai_energy_window(observed_utc DESC);
