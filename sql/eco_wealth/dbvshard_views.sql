-- filename: dbvshard_views.sql
-- destination: eco_restoration_shard/sql/eco_wealth/dbvshard_views.sql

PRAGMA foreign_keys = ON;

-- Base table for eco-wealth statements, one row per workload or corridor.
CREATE TABLE IF NOT EXISTS steward_eco_wealth_statement (
    statement_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    workload_id         TEXT NOT NULL,
    region              TEXT NOT NULL,
    plane_contract_id   TEXT NOT NULL,
    -- K,E,R aggregates for the workload window.
    k_avg               REAL NOT NULL CHECK (k_avg >= 0.0 AND k_avg <= 1.0),
    e_avg               REAL NOT NULL CHECK (e_avg >= 0.0 AND e_avg <= 1.0),
    r_avg               REAL NOT NULL CHECK (r_avg >= 0.0 AND r_avg <= 1.0),
    vt_max              REAL NOT NULL CHECK (vt_max >= 0.0),
    -- Eco wealth coordinates.
    eco_gain            REAL NOT NULL,
    energy_cost_kwh     REAL NOT NULL,
    eco_efficiency      REAL NOT NULL,
    -- Reward corridor and caps.
    reward_class        TEXT NOT NULL CHECK (
        reward_class IN ('BASELINE', 'GREEN', 'GOLD')
    ),
    eco_reward_cap      REAL NOT NULL CHECK (eco_reward_cap >= 0.0),
    karma_cap           REAL NOT NULL CHECK (karma_cap >= 0.0),
    reputation_cap      REAL NOT NULL CHECK (reputation_cap >= 0.0),
    -- Anchor into MT6883 healthcare vault when applicable.
    mt6883_course_id    TEXT,
    roh_anchor_hex      TEXT,
    created_utc         TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_steward_eco_wealth_statement_workload
    ON steward_eco_wealth_statement(workload_id, region);

-- View shard projecting eco-wealth statements into a vshard-friendly form
-- keyed by region, plane, and reward class.
CREATE VIEW IF NOT EXISTS vshard_steward_eco_wealth AS
SELECT
    region,
    plane_contract_id,
    reward_class,
    COUNT(*)                      AS n_workloads,
    AVG(eco_gain)                 AS avg_eco_gain,
    AVG(energy_cost_kwh)          AS avg_energy_cost_kwh,
    AVG(eco_efficiency)           AS avg_eco_efficiency,
    AVG(k_avg)                    AS k_band,
    AVG(e_avg)                    AS e_band,
    AVG(r_avg)                    AS r_band,
    MIN(eco_reward_cap)           AS min_eco_reward_cap,
    MAX(eco_reward_cap)           AS max_eco_reward_cap
FROM steward_eco_wealth_statement
GROUP BY region, plane_contract_id, reward_class;
