-- filename: eco_restoration_shard/cyboquatic_progress/20260713/sql/cyboquatic_workload_energyreq_dvt.sql
-- domain: (d) Cyboquatic workload telemetry in SQLite
-- purpose: Maintain cyboquatic_daily_progress.sqlite daily_progress rows for 2026-07-13.

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
  renergy           REAL    NOT NULL,
  rhydraulic        REAL    NOT NULL,
  runcertainty      REAL    NOT NULL,
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

-- Example Phoenix workload sample for 2026-07-13.
-- Energetics chosen to represent a strong tailwind window with safe hydraulics and moderate uncertainty.

INSERT INTO daily_progress (
  yyyymmdd, domain, subtask_id,
  node_id, sample_id, timestamp_utc,
  energy_req_j, energy_surplus_j,
  hydraulic_risk, uncertainty_risk,
  renergy, rhydraulic, runcertainty,
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
  0.0,        -- renergy (strong tailwind mapped to ~0)
  0.15,       -- rhydraulic (clamped safe)
  0.35,       -- runcertainty
  0.18,       -- vt_before (previous residual, non-negative)
  0.8 * (0.0 * 0.0) +
    1.0 * (0.15 * 0.15) +
    0.6 * (0.35 * 0.35), -- vt_after = W_ENERGY*re^2 + WHYDRAULIC*rh^2 + W_UNCERTAINTY*ru^2
  (0.8 * (0.0 * 0.0) +
    1.0 * (0.15 * 0.15) +
    0.6 * (0.35 * 0.35)) - 0.18, -- delta_vt
  CASE
    WHEN (0.8 * (0.0 * 0.0) +
          1.0 * (0.15 * 0.15) +
          0.6 * (0.35 * 0.35)) -
         0.18 > 0.0
      THEN GREATEST(
        LEAST(0.95 - 0.4 * GREATEST(0.0, 0.15, 0.35) - 0.25, 1.0),
        0.0
      )
    ELSE GREATEST(
      LEAST(0.95 - 0.4 * GREATEST(0.0, 0.15, 0.35), 1.0),
      0.0
    )
  END, -- k_factor
  CASE
    WHEN (0.8 * (0.0 * 0.0) +
          1.0 * (0.15 * 0.15) +
          0.6 * (0.35 * 0.35)) -
         0.18 > 0.0
      THEN GREATEST(
        LEAST(0.95 - (0.8 * (0.0 * 0.0) +
                      1.0 * (0.15 * 0.15) +
                      0.6 * (0.35 * 0.35)) - 0.3, 1.0),
        0.0
      )
    ELSE GREATEST(
      LEAST(0.95 - (0.8 * (0.0 * 0.0) +
                    1.0 * (0.15 * 0.15) +
                    0.6 * (0.35 * 0.35)), 1.0),
      0.0
    )
  END, -- e_factor
  CASE
    WHEN (0.8 * (0.0 * 0.0) +
          1.0 * (0.15 * 0.15) +
          0.6 * (0.35 * 0.35)) -
         0.18 > 0.0
      THEN LEAST(
        GREATEST(
          (0.8 * (0.0 * 0.0) +
           1.0 * (0.15 * 0.15) +
           0.6 * (0.35 * 0.35)) +
          ((0.8 * (0.0 * 0.0) +
            1.0 * (0.15 * 0.15) +
            0.6 * (0.35 * 0.35)) - 0.18),
          0.0
        ),
        1.0
      )
    ELSE LEAST(
      GREATEST(
        (0.8 * (0.0 * 0.0) +
         1.0 * (0.15 * 0.15) +
         0.6 * (0.35 * 0.35)),
        0.0
      ),
      1.0
    )
  END, -- r_factor
  '0x20260713PHX3345NWorkloadEnergyDeltaVtSql',
  '20260709/workload_energy_dvt_rust'
);

-- Next-step research hint:
-- - Tighten ENERGY_TAILWIND_SAFE_RATIO band using Phoenix grid-tailwind traces.
-- - Add canal velocity and sensor health planes to this table and extend residual
--   to include rvelocity, rcalib, rsigma in the same Lyapunov grammar.
