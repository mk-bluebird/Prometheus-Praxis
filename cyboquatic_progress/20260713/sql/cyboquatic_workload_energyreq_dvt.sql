-- filename: eco_restoration_shard/cyboquatic_progress/20260713/sql/cyboquatic_workload_energyreq_dvt.sql
-- domain: (d) Cyboquatic workload telemetry in SQLite
-- purpose: Maintain cyboquatic_daily_progress.sqlite daily_progress rows for 2026-07-13.
-- 
-- EXTENDED SCHEMA: Includes canal_velocity_mps, sensor_health, rvelocity, rsensor_health
-- as per next-step research questions in README.md and AI_CHAT_INTEGRATION.md

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS daily_progress (
  progress_id       INTEGER PRIMARY KEY AUTOINCREMENT,
  yyyymmdd          TEXT    NOT NULL,
  domain            TEXT    NOT NULL,
  subtask_id        TEXT    NOT NULL,
  node_id           TEXT    NOT NULL,
  sample_id         TEXT    NOT NULL,
  timestamp_utc     TEXT    NOT NULL,
  energy_req_j      REAL    NOT NULL,
  energy_surplus_j  REAL    NOT NULL,
  hydraulic_risk    REAL    NOT NULL,
  uncertainty_risk  REAL    NOT NULL,
  canal_velocity_mps REAL   DEFAULT 0.0,
  sensor_health     REAL    DEFAULT 1.0,
  renergy           REAL    NOT NULL,
  rhydraulic        REAL    NOT NULL,
  runcertainty      REAL    NOT NULL,
  rvelocity         REAL    DEFAULT 0.0,
  rsensor_health    REAL    DEFAULT 0.0,
  vt_before         REAL    NOT NULL,
  vt_after          REAL    NOT NULL,
  delta_vt          REAL    NOT NULL,
  k_factor          REAL    NOT NULL,
  e_factor          REAL    NOT NULL,
  r_factor          REAL    NOT NULL,
  phoenix_hex       TEXT    NOT NULL,
  prior_pointer     TEXT    NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_daily_progress_date
  ON daily_progress (yyyymmdd);

CREATE INDEX IF NOT EXISTS idx_daily_progress_node_time
  ON daily_progress (node_id, timestamp_utc);

-- =============================================================================
-- Views for AI-chat platforms and coding-agents
-- =============================================================================

-- View: Per-node workload summary over time windows
CREATE VIEW IF NOT EXISTS v_cybo_workload_window AS
SELECT 
    node_id,
    yyyymmdd,
    COUNT(*) as sample_count,
    AVG(energy_req_j) as avg_energy_req_j,
    AVG(energy_surplus_j) as avg_energy_surplus_j,
    AVG(renergy) as avg_renergy,
    AVG(rhydraulic) as avg_rhydraulic,
    AVG(runcertainty) as avg_runcertainty,
    AVG(rvelocity) as avg_rvelocity,
    AVG(rsensor_health) as avg_rsensor_health,
    AVG(vt_after) as avg_vt_after,
    AVG(k_factor) as avg_k_factor,
    AVG(e_factor) as avg_e_factor,
    AVG(r_factor) as avg_r_factor,
    MAX(delta_vt) as max_delta_vt,
    MIN(k_factor) as min_k_factor,
    MIN(e_factor) as min_e_factor,
    MAX(r_factor) as max_r_factor
FROM daily_progress
GROUP BY node_id, yyyymmdd;

-- View: Safe workload candidates (K>=0.9, E>=0.9, R<=0.15, ΔVt<=0)
-- These are candidates for always-improve routing per EcoNet spine work
CREATE VIEW IF NOT EXISTS v_safe_workload_candidates AS
SELECT *
FROM daily_progress
WHERE k_factor >= 0.9 
  AND e_factor >= 0.9 
  AND r_factor <= 0.15
  AND delta_vt <= 0.0;

-- View: Energy tailwind corridor analysis
CREATE VIEW IF NOT EXISTS v_tailwind_corridor AS
SELECT 
    node_id,
    timestamp_utc,
    energy_req_j,
    energy_surplus_j,
    CASE 
        WHEN energy_req_j > 0 THEN energy_surplus_j / energy_req_j
        ELSE 2.5
    END as tailwind_ratio,
    renergy,
    CASE 
        WHEN energy_req_j > 0 AND energy_surplus_j / energy_req_j >= 1.2 THEN 'SAFE'
        WHEN energy_req_j > 0 AND energy_surplus_j / energy_req_j <= 0.0 THEN 'CRITICAL'
        ELSE 'MARGINAL'
    END as corridor_status,
    k_factor,
    e_factor,
    r_factor,
    delta_vt
FROM daily_progress
ORDER BY timestamp_utc DESC;

-- =============================================================================
-- Example Phoenix workload sample for 2026-07-13
-- Energetics chosen to represent a strong tailwind window with safe hydraulics
-- and moderate uncertainty, including extended metrics
-- =============================================================================

INSERT INTO daily_progress (
  yyyymmdd, domain, subtask_id,
  node_id, sample_id, timestamp_utc,
  energy_req_j, energy_surplus_j,
  hydraulic_risk, uncertainty_risk,
  canal_velocity_mps, sensor_health,
  renergy, rhydraulic, runcertainty,
  rvelocity, rsensor_health,
  vt_before, vt_after, delta_vt,
  k_factor, e_factor, r_factor,
  phoenix_hex, prior_pointer
) VALUES (
  '20260713',
  'workload_energy_dvt',
  'PHX-CANAL-WL-2026-07-13',
  'PHX-CANAL-NODE-WL-02',
  'PHX-WL-SAMPLE-0002',
  '2026-07-13T233500Z',
  6.0,        -- energy_req_j
  8.4,        -- energy_surplus_j (ratio 1.4, strong tailwind)
  0.15,       -- hydraulic_risk (safe corridor)
  0.35,       -- uncertainty_risk (moderate sensor/model risk)
  1.2,        -- canal_velocity_mps (within safe threshold)
  0.92,       -- sensor_health (good sensor condition)
  0.0,        -- renergy (strong tailwind mapped to ~0)
  0.15,       -- rhydraulic (clamped safe)
  0.35,       -- runcertainty
  0.0,        -- rvelocity (velocity within threshold)
  0.08,       -- rsensor_health (1.0 - 0.92)
  0.18,       -- vt_before (previous residual, non-negative)
  -- vt_after computed as: W_ENERGY*re² + WHYDRAULIC*rh² + W_UNCERTAINTY*ru² + W_VELOCITY*rv² + W_SENSOR_HEALTH*rsh²
  0.8 * (0.0 * 0.0) +
    1.0 * (0.15 * 0.15) +
    0.6 * (0.35 * 0.35) +
    0.7 * (0.0 * 0.0) +
    0.5 * (0.08 * 0.08),
  -- delta_vt
  (0.8 * (0.0 * 0.0) +
    1.0 * (0.15 * 0.15) +
    0.6 * (0.35 * 0.35) +
    0.7 * (0.0 * 0.0) +
    0.5 * (0.08 * 0.08)) - 0.18,
  -- k_factor (with delta_vt penalty if positive)
  CASE
    WHEN (0.8 * (0.0 * 0.0) +
          1.0 * (0.15 * 0.15) +
          0.6 * (0.35 * 0.35) +
          0.7 * (0.0 * 0.0) +
          0.5 * (0.08 * 0.08)) -
         0.18 > 0.0
      THEN GREATEST(
        LEAST(0.95 - 0.4 * GREATEST(0.0, 0.15, 0.35, 0.0, 0.08) - 0.25, 1.0),
        0.0
      )
    ELSE GREATEST(
      LEAST(0.95 - 0.4 * GREATEST(0.0, 0.15, 0.35, 0.0, 0.08), 1.0),
      0.0
    )
  END,
  -- e_factor (with delta_vt penalty if positive)
  CASE
    WHEN (0.8 * (0.0 * 0.0) +
          1.0 * (0.15 * 0.15) +
          0.6 * (0.35 * 0.35) +
          0.7 * (0.0 * 0.0) +
          0.5 * (0.08 * 0.08)) -
         0.18 > 0.0
      THEN GREATEST(
        LEAST(0.95 - (0.8 * (0.0 * 0.0) +
                      1.0 * (0.15 * 0.15) +
                      0.6 * (0.35 * 0.35) +
                      0.7 * (0.0 * 0.0) +
                      0.5 * (0.08 * 0.08)) - 0.3, 1.0),
        0.0
      )
    ELSE GREATEST(
      LEAST(0.95 - (0.8 * (0.0 * 0.0) +
                    1.0 * (0.15 * 0.15) +
                    0.6 * (0.35 * 0.35) +
                    0.7 * (0.0 * 0.0) +
                    0.5 * (0.08 * 0.08)), 1.0),
      0.0
    )
  END,
  -- r_factor (with delta_vt addition if positive)
  CASE
    WHEN (0.8 * (0.0 * 0.0) +
          1.0 * (0.15 * 0.15) +
          0.6 * (0.35 * 0.35) +
          0.7 * (0.0 * 0.0) +
          0.5 * (0.08 * 0.08)) -
         0.18 > 0.0
      THEN LEAST(
        GREATEST(
          (0.8 * (0.0 * 0.0) +
           1.0 * (0.15 * 0.15) +
           0.6 * (0.35 * 0.35) +
           0.7 * (0.0 * 0.0) +
           0.5 * (0.08 * 0.08)) +
          ((0.8 * (0.0 * 0.0) +
            1.0 * (0.15 * 0.15) +
            0.6 * (0.35 * 0.35) +
            0.7 * (0.0 * 0.0) +
            0.5 * (0.08 * 0.08)) - 0.18),
          0.0
        ),
        1.0
      )
    ELSE LEAST(
      GREATEST(
        (0.8 * (0.0 * 0.0) +
         1.0 * (0.15 * 0.15) +
         0.6 * (0.35 * 0.35) +
         0.7 * (0.0 * 0.0) +
         0.5 * (0.08 * 0.08)),
        0.0
      ),
      1.0
    )
  END,
  '0x20260713PHX3345NWorkloadEnergyDeltaVtSqlExtended',
  '20260709/workload_energy_dvt_rust'
);

-- =============================================================================
-- Next-step research hints (from README.md)
-- =============================================================================
-- - Calibrate ENERGY_TAILWIND_SAFE_RATIO using Phoenix grid-tailwind traces
-- - Extend workload residual to include canal-velocity (rvelocity) and 
--   sensor health (rcalib, rsigma) planes for PFAS fate corridors
-- - Replay historical cyboquatic workloads to validate K,E,R gating rules
--   (K>=0.9, E>=0.9, R<=0.15) before production coupling
-- - Wire CI guards so that any new machinery shard entering EXPPROD or PROD
--   must show recent, Lyapunov-safe evidence rows in daily_progress
