-- filename: db_host_budget_healthcare.sql
-- destination: ecorestorationshard/db/db_host_budget_healthcare.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS host_healthcare_budget (
    host_address           TEXT PRIMARY KEY,
    base_energy_budget_kw  REAL NOT NULL,
    max_energy_budget_kw   REAL NOT NULL,
    current_energy_budget_kw REAL NOT NULL,
    last_update_utc        TEXT NOT NULL
);

-- Daily evolution loop rule for healthcare-only energy budget.
CREATE VIEW IF NOT EXISTS v_host_healthcare_budget_policy AS
SELECT
    w.host_address,
    w.eco_wealth_score,
    b.base_energy_budget_kw,
    b.max_energy_budget_kw,
    b.current_energy_budget_kw,
    CASE
        WHEN w.eco_wealth_score >= 1000 THEN
            MIN(b.max_energy_budget_kw, b.base_energy_budget_kw * 1.5)
        WHEN w.eco_wealth_score >= 500 THEN
            MIN(b.max_energy_budget_kw, b.base_energy_budget_kw * 1.2)
        ELSE
            b.base_energy_budget_kw
    END AS recommended_healthcare_budget_kw
FROM v_host_eco_wealth_score AS w
JOIN host_healthcare_budget AS b
    ON w.host_address = b.host_address;
