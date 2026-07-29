-- filename: ecorestorationshard/cyboquatic_progress/20260728/sql/cyboquatic_dailyprogress_seed.sql
-- purpose: SQL shard for domain (d) cyboquatic workload, KER and Canal node parameters
-- DB: dbcyboquaticdailyprogress.sqlite
-- hex-anchor: evidencehex = 0x20260728PHXWORKLOADENERGYDV
-- signingdid: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7

PRAGMA foreign_keys = ON;

-- 1. Extend dailyprogress with KER and canal node parameters (if not already present)
CREATE TABLE IF NOT EXISTS dailyprogress (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  domain_id TEXT NOT NULL,
  subtask_id TEXT NOT NULL,
  yyyymmdd TEXT NOT NULL,
  node_id TEXT NOT NULL,
  energyreqj REAL NOT NULL,
  r_energy REAL NOT NULL,
  r_hydraulics REAL NOT NULL,
  r_carbon REAL NOT NULL,
  r_uncertainty REAL NOT NULL,
  vt_before REAL NOT NULL,
  vt_after REAL NOT NULL,
  delta_vt REAL NOT NULL,
  k_metric REAL NOT NULL,
  e_metric REAL NOT NULL,
  r_metric REAL NOT NULL,
  canal_velocity_ms REAL NOT NULL,
  sensor_health_risk REAL NOT NULL,
  evidencehex TEXT NOT NULL,
  signingdid TEXT NOT NULL,
  priorpointerhex TEXT,
  createdutc TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_daily_domain_date
  ON dailyprogress(domain_id, yyyymmdd);

CREATE INDEX IF NOT EXISTS idx_daily_node_date
  ON dailyprogress(node_id, yyyymmdd);

-- 2. Seed one cyboquatic workload row for 2026-07-28
INSERT INTO dailyprogress (
  domain_id,
  subtask_id,
  yyyymmdd,
  node_id,
  energyreqj,
  r_energy,
  r_hydraulics,
  r_carbon,
  r_uncertainty,
  vt_before,
  vt_after,
  delta_vt,
  k_metric,
  e_metric,
  r_metric,
  canal_velocity_ms,
  sensor_health_risk,
  evidencehex,
  signingdid,
  priorpointerhex,
  createdutc
) VALUES (
  'd',
  'PHX-CANAL-WL-2026-07-28',
  '20260728',
  'PHX-CANAL-NODE-01',
  1000000.0,
  1.0,
  0.35,
  0.30,
  0.25,
  0.35,
  0.30,
  -0.05,
  0.90,
  0.85,
  0.15,
  0.50,
  0.10,
  '0x20260728PHXWORKLOADENERGYDV',
  'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
  '0x20260709PHX3345NWorkloadEnergyDeltaVt',
  '2026-07-28T00:00:00Z'
);
