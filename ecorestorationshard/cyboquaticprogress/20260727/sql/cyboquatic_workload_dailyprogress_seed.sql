-- filename: ecorestorationshard/cyboquaticprogress/20260727/sql/cyboquatic_workload_dailyprogress_seed.sql
-- purpose: Seed row and indices for cyboquatic workload energetics in db_cyboquaticdailyprogress.sqlite
-- domain: (d) Cyboquatic workload energetics
-- anchor: PHXWORKLOADENERGYDV20260727 / 0x20260727PHX3345NWorkloadEnergyDeltaVt

PRAGMA foreign_keys = ON;

-- Core table (aligned with 20260715 shard conventions). [file:13]
CREATE TABLE IF NOT EXISTS dailyprogress (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  yyyymmdd TEXT NOT NULL,
  domain TEXT NOT NULL,
  subtaskid TEXT NOT NULL,
  nodeid TEXT NOT NULL,
  energy_req_j REAL NOT NULL,
  energy_surplus_j REAL NOT NULL,
  r_energy REAL NOT NULL,
  r_hydraulics REAL NOT NULL,
  r_uncertainty REAL NOT NULL,
  vt_before REAL NOT NULL,
  vt_after REAL NOT NULL,
  delta_vt REAL NOT NULL,
  k_metric REAL,
  e_metric REAL,
  r_metric REAL,
  evidencehex TEXT NOT NULL,
  prioranchorhex TEXT,
  createdutc TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_dailyprogress_day_domain
  ON dailyprogress (yyyymmdd, domain);

CREATE INDEX IF NOT EXISTS idx_dailyprogress_subtask
  ON dailyprogress (subtaskid);

-- Seed example non-actuating diagnostic row for 2026-07-27.
INSERT INTO dailyprogress (
  yyyymmdd, domain, subtaskid, nodeid,
  energy_req_j, energy_surplus_j,
  r_energy, r_hydraulics, r_uncertainty,
  vt_before, vt_after, delta_vt,
  k_metric, e_metric, r_metric,
  evidencehex, prioranchorhex, createdutc
) VALUES (
  '20260727',
  'CYBOQUATIC',
  'PHX-CANAL-WORKLOAD-ENERGYDV-20260727',
  'PHX-CANAL-SEG-A1',
  1.0e6,           -- energy_req_j [J]
  1.2e6,           -- energy_surplus_j [J]
  0.0,             -- r_energy (strong tailwind)
  0.3,             -- r_hydraulics
  0.2,             -- r_uncertainty
  0.0,             -- vt_before
  0.3*0.3*0.30 +   -- vt_after = w_h*rh^2 + w_u*ru^2 (energy plane zero)
  0.2*0.2*0.20,
  (0.3*0.3*0.30 + 0.2*0.2*0.20) - 0.0,
  NULL,
  NULL,
  NULL,
  '0x20260727PHX3345NWorkloadEnergyDeltaVt',
  '0x20260709PHX3345NWorkloadEnergyDeltaVt',
  strftime('%Y-%m-%dT%H:%M:%SZ','now')
);
