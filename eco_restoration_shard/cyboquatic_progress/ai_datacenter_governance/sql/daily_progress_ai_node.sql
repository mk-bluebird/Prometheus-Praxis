-- filename: eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/sql/daily_progress_ai_node.sql
-- purpose : Schema and daily seed INSERT for the AI data centre daily progress table.
--           This extends the existing cyboquatic daily_progress convention with AI‑specific columns.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS daily_progress_ai_node (
    progress_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    yyyymmdd           TEXT NOT NULL,                 -- date of the telemetry window
    node_id            TEXT NOT NULL,                 -- e.g., 'PHX-AI-DC-01'
    domain             TEXT NOT NULL DEFAULT 'AI_DATA_CENTER',

    -- 10‑axis raw telemetry (all required; NULL allowed only if sensor missing)
    core_energy_intensity_kWh_per_workload   REAL,    -- kWh / (10^6 tokens or inference)
    joules_per_inference                    REAL,
    pue                                      REAL,
    cue_kg_per_kWh                           REAL,
    tokens_per_second                        REAL,
    inferences_per_second                    REAL,
    utilisation_pct                          REAL,    -- fraction
    ere                                      REAL,    -- heat reuse effectiveness
    eco_task_ratio                           REAL,    -- fraction of energy on eco‑tasks
    wue_l_per_kWh                            REAL,
    materials_intensity_kgCO2e_per_TFLOPyr   REAL,
    topology_risk_score                      REAL,

    -- Derived Lyapunov residual and ΔVt
    vt_residual           REAL NOT NULL,
    vt_previous           REAL NOT NULL,
    delta_vt              REAL NOT NULL,

    -- K,E,R triad
    k_factor              REAL NOT NULL,
    e_factor              REAL NOT NULL,
    r_factor              REAL NOT NULL,

    -- Evidence chain
    evidence_hex          TEXT NOT NULL,
    prior_pointer         TEXT NOT NULL,

    created_utc           TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_ai_node_date_node
    ON daily_progress_ai_node(yyyymmdd, node_id);

-- Seed row for 2026‑07‑16 (example values; replace with real telemetry)
INSERT INTO daily_progress_ai_node (
    yyyymmdd,
    node_id,
    core_energy_intensity_kWh_per_workload,
    joules_per_inference,
    pue,
    cue_kg_per_kWh,
    tokens_per_second,
    inferences_per_second,
    utilisation_pct,
    ere,
    eco_task_ratio,
    wue_l_per_kWh,
    materials_intensity_kgCO2e_per_TFLOPyr,
    topology_risk_score,
    vt_residual,
    vt_previous,
    delta_vt,
    k_factor,
    e_factor,
    r_factor,
    evidence_hex,
    prior_pointer
) VALUES (
    '20260716',
    'PHX-AI-DC-01',
    1.2,
    350.0,
    1.15,
    0.1,
    50000,
    1200,
    0.82,
    0.4,
    0.6,
    1.1,
    0.3,
    0.05,
    0.0725,    -- example Vt
    0.08,
    -0.0075,
    0.92,
    0.88,
    0.065,
    '0x20260716PHXAIDCNODE01SEED',
    '0x20260715PHXENERGYREQDV'
);
