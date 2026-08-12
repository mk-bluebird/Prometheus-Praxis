-- File: sql/eco_restoration/dynamic_energy_reference.sql

CREATE TABLE IF NOT EXISTS grid_carbon_hour (
    observed_unix_s INTEGER PRIMARY KEY,
    carbon_g_per_kwh REAL NOT NULL CHECK(carbon_g_per_kwh >= 0)
) STRICT;

CREATE TABLE IF NOT EXISTS fiscal_ecological_budget (
    budget_id INTEGER PRIMARY KEY CHECK(budget_id = 1),
    fiscal_start_unix_s INTEGER NOT NULL,
    fiscal_end_unix_s INTEGER NOT NULL CHECK(fiscal_end_unix_s > fiscal_start_unix_s),
    approved_carbon_g REAL NOT NULL CHECK(approved_carbon_g >= 0)
) STRICT;

CREATE TABLE IF NOT EXISTS ecological_carbon_ledger (
    event_id INTEGER PRIMARY KEY,
    observed_unix_s INTEGER NOT NULL,
    carbon_g REAL NOT NULL
) STRICT;

CREATE TABLE IF NOT EXISTS dynamic_energy_reference (
    observed_unix_s INTEGER PRIMARY KEY,
    rolling_carbon_g_per_kwh REAL NOT NULL,
    remaining_budget_g REAL NOT NULL,
    hourly_budget_g REAL NOT NULL,
    reference_energy_kwh REAL NOT NULL CHECK(reference_energy_kwh >= 0)
) STRICT;

CREATE VIEW IF NOT EXISTS current_energy_reference_hour AS
WITH fiscal AS (
    SELECT fiscal_start_unix_s, fiscal_end_unix_s, approved_carbon_g
    FROM fiscal_ecological_budget
    WHERE budget_id = 1
),
annual_grid AS (
    SELECT AVG(carbon_g_per_kwh) AS rolling_carbon_g_per_kwh
    FROM grid_carbon_hour
    WHERE observed_unix_s >= unixepoch() - 31536000
),
ledger AS (
    SELECT COALESCE(SUM(carbon_g), 0.0) AS used_carbon_g
    FROM ecological_carbon_ledger, fiscal
    WHERE observed_unix_s BETWEEN fiscal.fiscal_start_unix_s AND fiscal.fiscal_end_unix_s
)
SELECT
    unixepoch() AS observed_unix_s,
    annual_grid.rolling_carbon_g_per_kwh,
    MAX(0.0, fiscal.approved_carbon_g - ledger.used_carbon_g) AS remaining_budget_g,
    MAX(0.0, fiscal.approved_carbon_g - ledger.used_carbon_g) /
        MAX(1.0, (fiscal.fiscal_end_unix_s - unixepoch()) / 3600.0) AS hourly_budget_g,
    MAX(0.0, fiscal.approved_carbon_g - ledger.used_carbon_g) /
        MAX(1.0, (fiscal.fiscal_end_unix_s - unixepoch()) / 3600.0) /
        MAX(1e-9, annual_grid.rolling_carbon_g_per_kwh) AS reference_energy_kwh
FROM fiscal, annual_grid, ledger;
