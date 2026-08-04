-- File: sql/cold_survival_corridor.sql
PRAGMA foreign_keys = ON;

-- Monitoring schema for cold-survival dynamics in canal corridors.
CREATE TABLE IF NOT EXISTS cold_survival_corridor (
    corridor_id INTEGER PRIMARY KEY AUTOINCREMENT,
    canal_segment TEXT NOT NULL,
    timestamp_s REAL NOT NULL,
    lyapunov_exponent REAL NOT NULL,   -- estimated λ_max
    cold_survival_flag INTEGER NOT NULL, -- 1 = cold-survival, 0 = not
    pfas_half_life_days REAL NOT NULL,
    did TEXT NOT NULL,                  -- governance signature (DID)
    CHECK (cold_survival_flag IN (0,1)),
    FOREIGN KEY(did) REFERENCES governance_particle(did)
);

CREATE INDEX IF NOT EXISTS idx_cold_survival_segment_time
    ON cold_survival_corridor(canal_segment, timestamp_s);

-- hex_thermal_recovery assumed from earlier:
-- hex_thermal_recovery(h3_index, basin_id, date_utc, morning_lst_k, afternoon_lst_k,
--                      albedo, cooling_degree_hours)

-- View to join cold_survival_corridor with hex_thermal_recovery,
-- identifying segments where cold-survival coincides with high heat-island stress.
CREATE VIEW IF NOT EXISTS cold_survival_heat_stress AS
SELECT
    c.canal_segment,
    c.timestamp_s,
    c.lyapunov_exponent,
    c.cold_survival_flag,
    c.pfas_half_life_days,
    c.did,
    h.h3_index,
    h.basin_id,
    h.date_utc,
    h.morning_lst_k,
    h.afternoon_lst_k,
    h.albedo,
    h.cooling_degree_hours
FROM cold_survival_corridor c
JOIN hex_thermal_recovery h
  ON h.basin_id = c.canal_segment
 -- Align dates by truncating timestamp_s to date_utc; for simplicity, assume UTC seconds map directly.
WHERE c.cold_survival_flag = 1
  AND h.cooling_degree_hours > 500.0; -- high heat-island stress threshold

-- Usage:
-- SELECT * FROM cold_survival_heat_stress
-- ORDER BY canal_segment, timestamp_s;
