-- filename: ecorestorationshard/cyboquaticprogress/20260719/sql/dbcyboquaticdailyprogress_20260719.sql
-- destination: ecorestorationshard/cyboquaticprogress/20260719/sql/dbcyboquaticdailyprogress_20260719.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
-- domain (rotation): (d) Cyboquatic workload (energyreqJ, ΔVt)
-- evidence hex anchor (registered via phoenixhexregistry): 0x20260719PHXWORKLOADENERGYDV
-- This shard extends the shared dbcyboquaticdailyprogress.sqlite pattern for the 2026‑07‑19
-- workload domain: energy requirements and Lyapunov residual deltas for non‑actuating
-- cyboquatic industrial machinery in Phoenix canals and trays. [file:2]

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Daily workload telemetry table for 2026‑07‑19 (Cyboquatic workload)
--    KER, energyreqJ, ΔVt, and Phoenix hex anchoring. [file:2][file:32]
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS dailyprogress_workload_20260719 (
    -- Identity and linkage
    rowid              INTEGER PRIMARY KEY AUTOINCREMENT,
    workday_yyyymmdd   TEXT    NOT NULL,          -- "20260719"
    subtask_id         TEXT    NOT NULL,          -- e.g. "PHX-CANAL-WL-2026-07-19"
    node_id            TEXT    NOT NULL,          -- cyboquatic node identifier
    region_code        TEXT    NOT NULL,          -- e.g. "PHX-CAZ-CEIM"
    lane               TEXT    NOT NULL,          -- "RESEARCH", "EXP", never "PROD" for this tree [file:2]

    -- Workload energetics
    energyreq_j        REAL    NOT NULL,          -- Joules required for workload window [file:2]
    energy_j_baseline  REAL    NOT NULL,          -- Baseline Joules for prior comparable window
    delta_energy_j     REAL    NOT NULL,          -- energyreq_j - energy_j_baseline

    -- Lyapunov residuals and KER
    vt_prev            REAL    NOT NULL,          -- prior V(t) residual for this node [file:32]
    vt_curr            REAL    NOT NULL,          -- current residual V(t) after workload [file:32]
    delta_vt           REAL    NOT NULL,          -- vt_curr - vt_prev (ΔVt) [file:2][file:32]
    k_factor           REAL    NOT NULL,          -- Knowledge factor in [0,1] [file:2]
    e_factor           REAL    NOT NULL,          -- Eco-impact factor in [0,1] [file:2]
    r_factor           REAL    NOT NULL,          -- Risk-of-harm factor in [0,1] [file:2]

    -- Planes and normalized risks (0–1 corridors) [file:2][file:32]
    r_energy           REAL    NOT NULL,          -- normalized energy risk in [0,1]
    r_carbon           REAL    NOT NULL,          -- normalized carbon risk in [0,1]
    r_hydraulics       REAL    NOT NULL,          -- normalized hydraulics risk in [0,1]
    r_materials        REAL    NOT NULL,          -- normalized materials risk in [0,1]
    r_dataquality      REAL    NOT NULL,          -- normalized ingest/sensor risk in [0,1]

    -- Phoenix hex anchoring, governance, and chaining [file:2][file:36]
    evidence_hex       TEXT    NOT NULL,          -- e.g. "0x20260719PHXWORKLOADENERGYDV"
    hex_logical_name   TEXT    NOT NULL,          -- e.g. "PHXWORKLOADENERGYDV20260719"
    signing_did        TEXT    NOT NULL,          -- "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7"
    prior_evidence_hex TEXT,                      -- previous day's workload hex (20260709) [file:2]
    created_utc        TEXT    NOT NULL,          -- ISO‑8601 timestamp
    notes              TEXT,

    -- Invariants (soft flags, no actuation) [file:2][file:13]
    ker_deployable     INTEGER NOT NULL DEFAULT 0 CHECK (ker_deployable IN (0,1)),
    vt_contractive     INTEGER NOT NULL DEFAULT 0 CHECK (vt_contractive IN (0,1)),
    non_actuating      INTEGER NOT NULL DEFAULT 1 CHECK (non_actuating IN (0,1))
);

CREATE INDEX IF NOT EXISTS idx_dailyprogress_workload_20260719_day
    ON dailyprogress_workload_20260719 (workday_yyyymmdd, region_code, lane);

CREATE INDEX IF NOT EXISTS idx_dailyprogress_workload_20260719_hex
    ON dailyprogress_workload_20260719 (evidence_hex, hex_logical_name);

----------------------------------------------------------------------
-- 2. Seed row for 2026‑07‑19 workload subtask PHX-CANAL-WL-2026-07-19
--    Non‑actuating diagnostic corridor: energyreqJ, ΔVt, and K,E,R triad. [file:2][file:32]
----------------------------------------------------------------------

INSERT INTO dailyprogress_workload_20260719 (
    workday_yyyymmdd,
    subtask_id,
    node_id,
    region_code,
    lane,
    energyreq_j,
    energy_j_baseline,
    delta_energy_j,
    vt_prev,
    vt_curr,
    delta_vt,
    k_factor,
    e_factor,
    r_factor,
    r_energy,
    r_carbon,
    r_hydraulics,
    r_materials,
    r_dataquality,
    evidence_hex,
    hex_logical_name,
    signing_did,
    prior_evidence_hex,
    created_utc,
    notes,
    ker_deployable,
    vt_contractive,
    non_actuating
) VALUES (
    '20260719',
    'PHX-CANAL-WL-2026-07-19',
    'PHX-CYBO-NODE-WL-001',
    'PHX-CAZ-CEIM',
    'RESEARCH',
    3.60e+05,              -- 360 kJ workload diagnostic (no actuation) [file:2]
    3.30e+05,              -- 330 kJ baseline
    3.00e+04,              -- Δenergy = 30 kJ
    0.50,                  -- prior residual V(t)
    0.45,                  -- current residual V(t) (contractive) [file:32]
    -0.05,                 -- ΔVt < 0 contractive corridor
    0.93,                  -- K: high knowledge factor [file:2]
    0.91,                  -- E: strong eco-impact potential [file:2]
    0.13,                  -- R: low risk-of-harm [file:13]
    0.22,                  -- r_energy in [0,1]
    0.18,                  -- r_carbon
    0.20,                  -- r_hydraulics
    0.15,                  -- r_materials
    0.10,                  -- r_dataquality
    '0x20260719PHXWORKLOADENERGYDV',
    'PHXWORKLOADENERGYDV20260719',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    '0x20260709PHX3345NWorkloadEnergyDeltaVt',
    '2026-07-19T23:32:00Z',
    'Non-actuating cyboquatic workload diagnostic for Phoenix canal energyreqJ and ΔVt corridor replay; tied to PHXWORKLOADENERGYDV20260719 anchor.',
    0,
    1,
    1
);
