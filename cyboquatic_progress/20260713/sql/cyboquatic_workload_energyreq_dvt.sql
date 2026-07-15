-- filename: eco_restoration_shard/cyboquatic_progress/20260713/sql/cyboquatic_workload_energyreq_dvt.sql
-- domain: (d) Cyboquatic workload telemetry in SQLite
-- purpose: Maintain cyboquatic_daily_progress.sqlite daily_progress rows for 2026-07-13
-- This script assumes SQLite 3.x and is designed for production-grade telemetry and AI-chat integration.

PRAGMA foreign_keys = ON;

-- Core workload/telemetry table for cyboquatic nodes
CREATE TABLE IF NOT EXISTS daily_progress (
  id                   INTEGER PRIMARY KEY AUTOINCREMENT,
  yyyymmdd             TEXT    NOT NULL,  -- YYYYMMDD (Phoenix-local date)
  domain               TEXT    NOT NULL,  -- e.g., workload_energy_dvt, drainagedecay, etc.
  subtask_id           TEXT    NOT NULL,  -- stable subtask tag per day/domain
  node_id              TEXT    NOT NULL,  -- physical or logical cyboquatic node identifier
  sample_id            TEXT    NOT NULL,  -- unique sample identifier per node
  timestamp_utc        TEXT    NOT NULL,  -- ISO-8601 UTC timestamp
  energy_req_j         REAL    NOT NULL,  -- required energy in Joules
  energy_surplus_j     REAL    NOT NULL,  -- surplus/available energy in Joules
  hydraulic_risk       REAL    NOT NULL,  -- 0.0–1.0, higher means higher hydraulic risk
  uncertainty_risk     REAL    NOT NULL,  -- 0.0–1.0, higher means higher uncertainty
  canal_velocity_mps   REAL    DEFAULT 0.0,  -- mean canal velocity in m/s
  sensor_health        REAL    DEFAULT 1.0,  -- 0.0–1.0, higher is better
  renergy              REAL    NOT NULL,  -- residual energy risk plane
  rhydraulic           REAL    NOT NULL,  -- residual hydraulic risk plane
  runcertainty         REAL    NOT NULL,  -- residual uncertainty risk plane
  rvelocity            REAL    DEFAULT 0.0,  -- residual velocity risk plane
  rsensor_health       REAL    DEFAULT 0.0,  -- residual sensor health plane
  vt_before            REAL    NOT NULL,  -- Lyapunov-like residual before update
  vt_after             REAL    NOT NULL,  -- Lyapunov-like residual after update
  delta_vt             REAL    NOT NULL,  -- vt_after - vt_before
  k_factor             REAL    NOT NULL,  -- knowledge factor (0–1)
  e_factor             REAL    NOT NULL,  -- eco-impact factor (0–1)
  r_factor             REAL    NOT NULL,  -- risk factor (0–1)
  phoenix_hex          TEXT    NOT NULL,  -- Phoenix-tagged evidence hex
  prior_pointer        TEXT    NOT NULL,  -- pointer to prior-day artifact root
  progress_date        TEXT,              -- optional YYYY-MM-DD for cross-day joins
  domain_code          TEXT,              -- optional a..g for domain cycle mapping
  k_score              REAL,              -- optional aggregate knowledge score
  e_score              REAL,              -- optional aggregate eco-impact score
  r_score              REAL,              -- optional aggregate risk score
  artifact_root        TEXT,              -- optional artifact root path
  notes                TEXT,              -- optional free-form notes
  research_queries     TEXT,              -- optional next-step research hints
  created_at           TEXT    DEFAULT (datetime('now','localtime'))
);

CREATE INDEX IF NOT EXISTS idx_daily_progress_date
  ON daily_progress (yyyymmdd);

CREATE INDEX IF NOT EXISTS idx_daily_progress_node_time
  ON daily_progress (node_id, timestamp_utc);

CREATE INDEX IF NOT EXISTS idx_daily_progress_domain
  ON daily_progress (domain);

CREATE INDEX IF NOT EXISTS idx_daily_progress_subtask
  ON daily_progress (subtask_id);

-- View: Per-node workload summary over time windows
CREATE VIEW IF NOT EXISTS v_cybo_workload_window AS
SELECT 
    node_id,
    yyyymmdd,
    COUNT(*) AS sample_count,
    AVG(energy_req_j) AS avg_energy_req_j,
    AVG(energy_surplus_j) AS avg_energy_surplus_j,
    AVG(renergy) AS avg_renergy,
    AVG(rhydraulic) AS avg_rhydraulic,
    AVG(runcertainty) AS avg_runcertainty,
    AVG(rvelocity) AS avg_rvelocity,
    AVG(rsensor_health) AS avg_rsensor_health,
    AVG(vt_after) AS avg_vt_after,
    AVG(k_factor) AS avg_k_factor,
    AVG(e_factor) AS avg_e_factor,
    AVG(r_factor) AS avg_r_factor,
    MAX(delta_vt) AS max_delta_vt,
    MIN(k_factor) AS min_k_factor,
    MIN(e_factor) AS min_e_factor,
    MAX(r_factor) AS max_r_factor
FROM daily_progress
GROUP BY node_id, yyyymmdd;

-- View: Safe workload candidates (K>=0.9, E>=0.9, R<=0.15, ΔVt<=0)
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
    END AS tailwind_ratio,
    renergy,
    CASE 
        WHEN energy_req_j > 0 AND energy_surplus_j / energy_req_j >= 1.2 THEN 'SAFE'
        WHEN energy_req_j > 0 AND energy_surplus_j / energy_req_j <= 0.0 THEN 'CRITICAL'
        ELSE 'MARGINAL'
    END AS corridor_status,
    k_factor,
    e_factor,
    r_factor,
    delta_vt
FROM daily_progress
ORDER BY timestamp_utc DESC;

-- Example Phoenix workload sample for 2026-07-13
-- Energetics represent a strong tailwind window with safe hydraulics and moderate uncertainty
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
  phoenix_hex, prior_pointer,
  progress_date, domain_code,
  k_score, e_score, r_score,
  artifact_root, notes, research_queries
) VALUES (
  '20260713',
  'workload_energy_dvt',
  'PHX-CANAL-WL-2026-07-13',
  'PHX-CANAL-NODE-WL-02',
  'PHX-WL-SAMPLE-0002',
  '2026-07-13T23:35:00Z',
  6.0,
  8.4,
  0.15,
  0.35,
  1.2,
  0.92,
  0.0,
  0.15,
  0.35,
  0.0,
  0.08,
  0.18,
  0.8 * (0.0 * 0.0) +
    1.0 * (0.15 * 0.15) +
    0.6 * (0.35 * 0.35) +
    0.7 * (0.0 * 0.0) +
    0.5 * (0.08 * 0.08),
  (0.8 * (0.0 * 0.0) +
    1.0 * (0.15 * 0.15) +
    0.6 * (0.35 * 0.35) +
    0.7 * (0.0 * 0.0) +
    0.5 * (0.08 * 0.08)) - 0.18,
  CASE
    WHEN (0.8 * (0.0 * 0.0) +
          1.0 * (0.15 * 0.15) +
          0.6 * (0.35 * 0.35) +
          0.7 * (0.0 * 0.0) +
          0.5 * (0.08 * 0.08)) -
         0.18 > 0.0
      THEN MAX(
        MIN(0.95 - 0.4 * MAX(0.0, 0.15, 0.35, 0.0, 0.08) - 0.25, 1.0),
        0.0
      )
    ELSE MAX(
      MIN(0.95 - 0.4 * MAX(0.0, 0.15, 0.35, 0.0, 0.08), 1.0),
      0.0
    )
  END,
  CASE
    WHEN (0.8 * (0.0 * 0.0) +
          1.0 * (0.15 * 0.15) +
          0.6 * (0.35 * 0.35) +
          0.7 * (0.0 * 0.0) +
          0.5 * (0.08 * 0.08)) -
         0.18 > 0.0
      THEN MAX(
        MIN(0.95 - (0.8 * (0.0 * 0.0) +
                      1.0 * (0.15 * 0.15) +
                      0.6 * (0.35 * 0.35) +
                      0.7 * (0.0 * 0.0) +
                      0.5 * (0.08 * 0.08)) - 0.3, 1.0),
        0.0
      )
    ELSE MAX(
      MIN(0.95 - (0.8 * (0.0 * 0.0) +
                    1.0 * (0.15 * 0.15) +
                    0.6 * (0.35 * 0.35) +
                    0.7 * (0.0 * 0.0) +
                    0.5 * (0.08 * 0.08)), 1.0),
      0.0
    )
  END,
  CASE
    WHEN (0.8 * (0.0 * 0.0) +
          1.0 * (0.15 * 0.15) +
          0.6 * (0.35 * 0.35) +
          0.7 * (0.0 * 0.0) +
          0.5 * (0.08 * 0.08)) -
         0.18 > 0.0
      THEN MIN(
        MAX(
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
    ELSE MIN(
      MAX(
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
  'eco_restoration_shard/cyboquatic_progress/20260709/workload_energy_dvt_rust',
  '2026-07-13',
  'd',
  0.91,
  0.93,
  0.14,
  'eco_restoration_shard/cyboquatic_progress/20260713/',
  'Cyboquatic workload energyreqJ and ΔVt sample for Phoenix canal node WL-02; tuned for strong energy tailwind, safe hydraulics and moderate uncertainty for Lyapunov corridor validation.',
  '["calibrate ENERGY_TAILWIND_SAFE_RATIO using Phoenix grid-tailwind traces","extend residual planes with canal-velocity and sensor calibration variance for PFAS fate corridors","replay historical workloads to validate K,E,R gating rules before production","wire CI guards requiring recent Lyapunov-safe rows in daily_progress for any EXPPROD/PROD machinery shard"]'
);
