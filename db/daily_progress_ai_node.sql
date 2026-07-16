-- filename: db/daily_progress_ai_node.sql
-- destination: Prometheus-Praxis/db/daily_progress_ai_node.sql

PRAGMA foreign_keys = ON;

-- Daily progress table for AI data center nodes treated as Cyboquatic assets.
-- One row per node per day (or other canonical window), aligned with AiDatacenterNode2026v1
-- and the existing KER / Lyapunov grammar.[file:91]

CREATE TABLE IF NOT EXISTS daily_progress_ai_node (
    -- Identity and governance
    nodeid                    TEXT    NOT NULL,
    region                    TEXT    NOT NULL,
    lane                      TEXT    NOT NULL,   -- RESEARCH | EXP | PROD
    steward_uuid              TEXT    NOT NULL,
    steward_signinghex        TEXT    NOT NULL,
    twindow_start             TEXT    NOT NULL,   -- ISO-8601
    twindow_end               TEXT    NOT NULL,   -- ISO-8601

    -- Raw telemetry (per canonical window)
    core_energy_kwh_per_workload REAL NOT NULL CHECK(core_energy_kwh_per_workload >= 0.0),
    joules_per_inference          REAL NOT NULL CHECK(joules_per_inference >= 0.0),
    pue                           REAL NOT NULL CHECK(pue >= 0.0),
    cue_kg_co2_per_kwh            REAL NOT NULL CHECK(cue_kg_co2_per_kwh >= 0.0),
    eco_per_joule                 REAL NOT NULL,
    throughput_tokens_per_s       REAL NOT NULL,
    throughput_inferences_per_s   REAL NOT NULL,
    utilization_pct               REAL NOT NULL CHECK(utilization_pct >= 0.0 AND utilization_pct <= 100.0),
    ere                           REAL NOT NULL,
    eco_task_ratio_pct            REAL NOT NULL,
    wue_l_per_kwh                 REAL NOT NULL CHECK(wue_l_per_kwh >= 0.0),
    embodied_kg_co2eq             REAL NOT NULL CHECK(embodied_kg_co2eq >= 0.0),
    topology_violation_count      INTEGER NOT NULL CHECK(topology_violation_count >= 0),

    -- Normalized risk coordinates r_j (0–1)
    r_pue             REAL NOT NULL CHECK(r_pue             >= 0.0 AND r_pue             <= 1.0),
    r_cue             REAL NOT NULL CHECK(r_cue             >= 0.0 AND r_cue             <= 1.0),
    r_eco_per_joule   REAL NOT NULL CHECK(r_eco_per_joule   >= 0.0 AND r_eco_per_joule   <= 1.0),
    r_eco_task_ratio  REAL NOT NULL CHECK(r_eco_task_ratio  >= 0.0 AND r_eco_task_ratio  <= 1.0),
    r_wue             REAL NOT NULL CHECK(r_wue             >= 0.0 AND r_wue             <= 1.0),
    r_embodied        REAL NOT NULL CHECK(r_embodied        >= 0.0 AND r_embodied        <= 1.0),
    r_topology        REAL NOT NULL CHECK(r_topology        >= 0.0 AND r_topology        <= 1.0),
    r_energy          REAL NOT NULL CHECK(r_energy          >= 0.0 AND r_energy          <= 1.0),
    r_joule_inf       REAL NOT NULL CHECK(r_joule_inf       >= 0.0 AND r_joule_inf       <= 1.0),
    r_heat_reuse      REAL NOT NULL CHECK(r_heat_reuse      >= 0.0 AND r_heat_reuse      <= 1.0),
    r_utilization     REAL NOT NULL CHECK(r_utilization     >= 0.0 AND r_utilization     <= 1.0),
    r_bandwidth       REAL NOT NULL CHECK(r_bandwidth       >= 0.0 AND r_bandwidth       <= 1.0),

    -- Lyapunov residual and KER
    vt                REAL NOT NULL CHECK(vt >= 0.0),
    k                 REAL NOT NULL CHECK(k  >= 0.0 AND k  <= 1.0),
    e                 REAL NOT NULL CHECK(e  >= 0.0 AND e  <= 1.0),
    r                 REAL NOT NULL CHECK(r  >= 0.0 AND r  <= 1.0),

    -- Composite strength index
    strength_index_s  REAL NOT NULL CHECK(strength_index_s >= 0.0 AND strength_index_s <= 1.0),

    -- Evidence and chaining
    evidencehex       TEXT    NOT NULL,
    prior_evidencehex TEXT,
    phoenix_anchor_id TEXT,             -- optional: reference to phoenix_hex_anchor.hex_id
    created_utc       TEXT    NOT NULL, -- ISO-8601

    PRIMARY KEY (nodeid, twindow_start)
);

CREATE INDEX IF NOT EXISTS idx_daily_ai_node_lane_time
    ON daily_progress_ai_node (lane, twindow_start);

CREATE INDEX IF NOT EXISTS idx_daily_ai_node_steward_time
    ON daily_progress_ai_node (steward_uuid, twindow_start);

CREATE INDEX IF NOT EXISTS idx_daily_ai_node_evidence
    ON daily_progress_ai_node (evidencehex);
