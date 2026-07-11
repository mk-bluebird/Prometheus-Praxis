-- filename: eco_restoration_shard/crates/cyboquatic_progress/20260709_crate/daily_progress_seed.sql

PRAGMA foreign_keys = ON;

-- Create daily_progress table if not present (for plain SQL migration environments).
CREATE TABLE IF NOT EXISTS daily_progress (
    progress_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    yyyymmdd         TEXT NOT NULL,
    crate_id         TEXT NOT NULL,
    domain           TEXT NOT NULL,
    subtask_id       TEXT NOT NULL,
    node_id          TEXT NOT NULL,
    sample_id        TEXT NOT NULL,
    timestamp_utc    TEXT NOT NULL,
    energy_req_j     REAL NOT NULL,
    energy_surplus_j REAL NOT NULL,
    hydraulic_risk   REAL NOT NULL,
    uncertainty_risk REAL NOT NULL,
    r_energy         REAL NOT NULL,
    r_hydraulic      REAL NOT NULL,
    r_uncertainty    REAL NOT NULL,
    vt_before        REAL NOT NULL,
    vt_after         REAL NOT NULL,
    delta_vt         REAL NOT NULL,
    k_factor         REAL NOT NULL,
    e_factor         REAL NOT NULL,
    r_factor         REAL NOT NULL,
    evidence_hex     TEXT NOT NULL,
    prior_crate_id   TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_daily_progress_date
    ON daily_progress (yyyymmdd);

CREATE INDEX IF NOT EXISTS idx_daily_progress_node_time
    ON daily_progress (node_id, timestamp_utc);

-- Seed one Phoenix workload-energy sample for 2026-07-09, consistent with the Rust normalization and KER logic.
-- Example: moderate workload with strong energy tailwind, hydraulically safe, medium uncertainty.

INSERT INTO daily_progress (
    yyyymmdd,
    crate_id,
    domain,
    subtask_id,
    node_id,
    sample_id,
    timestamp_utc,
    energy_req_j,
    energy_surplus_j,
    hydraulic_risk,
    uncertainty_risk,
    r_energy,
    r_hydraulic,
    r_uncertainty,
    vt_before,
    vt_after,
    delta_vt,
    k_factor,
    e_factor,
    r_factor,
    evidence_hex,
    prior_crate_id
) VALUES (
    '20260709',
    'cyboquatic_workload20260709',
    'workload_energy_delta_vt',
    'PHX-CANAL-WL-2026-07-09',
    'PHX-CANAL-NODE-WL-01',
    'PHX-WL-SAMPLE-0001',
    '2026-07-09T233100Z',
    5.0,    -- energy_req_j (J)
    7.0,    -- energy_surplus_j (J) -> ratio 1.4 >= SAFE, strong tailwind
    0.2,    -- hydraulic_risk (safe corridor)
    0.4,    -- uncertainty_risk (moderate sensor/model risk)
    0.0,    -- r_energy: tailwind mapped to 0 risk
    0.2,    -- r_hydraulic
    0.4,    -- r_uncertainty
    0.15,   -- vt_before (example residual from prior window)
    (0.8 * 0.0 * 0.0) + (1.0 * 0.2 * 0.2) + (0.6 * 0.4 * 0.4), -- vt_after = W_ENERGY*r_e^2 + W_HYDRAULIC*r_h^2 + W_UNCERTAINTY*r_u^2
    ((0.8 * 0.0 * 0.0) + (1.0 * 0.2 * 0.2) + (0.6 * 0.4 * 0.4)) - 0.15, -- delta_vt = vt_after - vt_before
    -- k_factor: 0.95 - 0.4 * max(r_energy, r_hydraulic, r_uncertainty), delta_vt assumed <= 0 so no extra penalty.
    CASE
        WHEN ((0.8 * 0.0 * 0.0) + (1.0 * 0.2 * 0.2) + (0.6 * 0.4 * 0.4)) - 0.15 <= 0.0
        THEN
            CASE
                WHEN 0.4 >= 0.0 AND 0.4 <= 1.0
                THEN
                    CASE
                        WHEN (0.95 - 0.4 * 0.4) < 0.0 THEN 0.0
                        WHEN (0.95 - 0.4 * 0.4) > 1.0 THEN 1.0
                        ELSE (0.95 - 0.4 * 0.4)
                    END
                ELSE 0.0
            END
        ELSE 0.0
    END,
    -- e_factor: 0.95 - vt; delta_vt <= 0 so no extra penalty.
    CASE
        WHEN (0.95 - ((0.8 * 0.0 * 0.0) + (1.0 * 0.2 * 0.2) + (0.6 * 0.4 * 0.4))) < 0.0 THEN 0.0
        WHEN (0.95 - ((0.8 * 0.0 * 0.0) + (1.0 * 0.2 * 0.2) + (0.6 * 0.4 * 0.4))) > 1.0 THEN 1.0
        ELSE (0.95 - ((0.8 * 0.0 * 0.0) + (1.0 * 0.2 * 0.2) + (0.6 * 0.4 * 0.4)))
    END,
    -- r_factor: vt_after + max(delta_vt,0); with delta_vt <= 0, r_factor = vt_after clipped to [0,1].
    CASE
        WHEN ((0.8 * 0.0 * 0.0) + (1.0 * 0.2 * 0.2) + (0.6 * 0.4 * 0.4)) < 0.0 THEN 0.0
        WHEN ((0.8 * 0.0 * 0.0) + (1.0 * 0.2 * 0.2) + (0.6 * 0.4 * 0.4)) > 1.0 THEN 1.0
        ELSE ((0.8 * 0.0 * 0.0) + (1.0 * 0.2 * 0.2) + (0.6 * 0.4 * 0.4))
    END,
    '0x20260709PHX3345NWorkloadEnergyDeltaVt',
    'cyboquaticdrainagedecay20260708'
);
