-- filename: crates/prometheus_praxis_ai/sql/db_prometheus_praxis_ai.sql

PRAGMA foreign_keys = ON;

-- Phoenix Hex registry bindings for prometheus_praxis_ai crate.
CREATE TABLE IF NOT EXISTS phoenixhexanchor (
    anchor_id           TEXT PRIMARY KEY,
    domain              TEXT NOT NULL,
    scope               TEXT NOT NULL,
    did_root            TEXT NOT NULL,
    created_utc         TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS phoenixhexfile (
    file_id             TEXT PRIMARY KEY,
    anchor_id           TEXT NOT NULL,
    path                TEXT NOT NULL,
    hash_hex            TEXT NOT NULL,
    created_utc         TEXT NOT NULL,
    FOREIGN KEY (anchor_id) REFERENCES phoenixhexanchor(anchor_id)
);

CREATE TABLE IF NOT EXISTS phoenixhexparticlebinding (
    binding_id          TEXT PRIMARY KEY,
    anchor_id           TEXT NOT NULL,
    particle_id         TEXT NOT NULL,
    aln_spec_name       TEXT NOT NULL,
    created_utc         TEXT NOT NULL,
    FOREIGN KEY (anchor_id) REFERENCES phoenixhexanchor(anchor_id)
);

-- Drainage band frames (hydraulics / drainage decay).
CREATE TABLE IF NOT EXISTS drainage_frame (
    frame_id            TEXT PRIMARY KEY,
    yyyymmdd            TEXT NOT NULL,
    canal_segment_id    TEXT NOT NULL,
    node_id             TEXT NOT NULL,
    bod_mg_l            REAL NOT NULL CHECK (bod_mg_l >= 0.0 AND bod_mg_l <= 80.0),
    tss_mg_l            REAL NOT NULL CHECK (tss_mg_l >= 0.0 AND tss_mg_l <= 500.0),
    cec_cmol_per_kg     REAL NOT NULL CHECK (cec_cmol_per_kg >= 0.0 AND cec_cmol_per_kg <= 50.0),
    flow_rate_m3s       REAL NOT NULL CHECK (flow_rate_m3s >= 0.0),
    water_temp_c        REAL NOT NULL CHECK (water_temp_c >= 0.0 AND water_temp_c <= 45.0),
    elevation_m         REAL NOT NULL CHECK (elevation_m >= -100.0 AND elevation_m <= 2000.0),
    r_bod               REAL NOT NULL CHECK (r_bod >= 0.0 AND r_bod <= 1.0),
    r_tss               REAL NOT NULL CHECK (r_tss >= 0.0 AND r_tss <= 1.0),
    r_cec               REAL NOT NULL CHECK (r_cec >= 0.0 AND r_cec <= 1.0),
    r_hydraulics        REAL NOT NULL CHECK (r_hydraulics >= 0.0 AND r_hydraulics <= 1.0),
    r_uncertainty       REAL NOT NULL CHECK (r_uncertainty >= 0.0 AND r_uncertainty <= 1.0),
    vt_before           REAL NOT NULL CHECK (vt_before >= 0.0),
    vt_after            REAL NOT NULL CHECK (vt_after >= 0.0),
    delta_vt            REAL NOT NULL,
    k                   REAL NOT NULL CHECK (k >= 0.0 AND k <= 1.0),
    e                   REAL NOT NULL CHECK (e >= 0.0 AND e <= 1.0),
    r                   REAL NOT NULL CHECK (r >= 0.0 AND r <= 1.0),
    kerScore            REAL NOT NULL CHECK (kerScore > 0.0),
    phoenix_hex_anchor  TEXT NOT NULL,
    prior_frame_id      TEXT
);

-- Workload band frames (cyboquatic workloads).
CREATE TABLE IF NOT EXISTS workload_frame (
    frame_id            TEXT PRIMARY KEY,
    yyyymmdd            TEXT NOT NULL,
    workload_id         TEXT NOT NULL,
    node_id             TEXT NOT NULL,
    task_type           TEXT NOT NULL,
    timestamputc        TEXT NOT NULL,
    energyreq_j         REAL NOT NULL CHECK (energyreq_j >= 0.0 AND energyreq_j <= 1.0e9),
    energysurplus_j     REAL NOT NULL CHECK (energysurplus_j >= 0.0),
    hydraulicrisk       REAL NOT NULL CHECK (hydraulicrisk >= 0.0 AND hydraulicrisk <= 1.0),
    uncertaintyrisk     REAL NOT NULL CHECK (uncertaintyrisk >= 0.0 AND uncertaintyrisk <= 1.0),
    r_energy            REAL NOT NULL CHECK (r_energy >= 0.0 AND r_energy <= 1.0),
    r_hydraulics        REAL NOT NULL CHECK (r_hydraulics >= 0.0 AND r_hydraulics <= 1.0),
    r_uncertainty       REAL NOT NULL CHECK (r_uncertainty >= 0.0 AND r_uncertainty <= 1.0),
    vt_before           REAL NOT NULL CHECK (vt_before >= 0.0),
    vt_after            REAL NOT NULL CHECK (vt_after >= 0.0),
    delta_vt            REAL NOT NULL,
    k                   REAL NOT NULL CHECK (k >= 0.0 AND k <= 1.0),
    e                   REAL NOT NULL CHECK (e >= 0.0 AND e <= 1.0),
    r                   REAL NOT NULL CHECK (r >= 0.0 AND r <= 1.0),
    kerScore            REAL NOT NULL CHECK (kerScore > 0.0),
    lane                TEXT NOT NULL CHECK (lane IN ('RESEARCH','PILOT','PRODUCTION')),
    phoenix_hex_anchor  TEXT NOT NULL,
    prior_frame_id      TEXT
);

-- AI node band frames (datacenter / AI node energetics).
CREATE TABLE IF NOT EXISTS ai_node_frame (
    frame_id                 TEXT PRIMARY KEY,
    yyyymmdd                 TEXT NOT NULL,
    facility_id              TEXT NOT NULL,
    rack_id                  TEXT NOT NULL,
    tile_id                  TEXT NOT NULL,
    timestamputc             TEXT NOT NULL,
    pue                      REAL NOT NULL CHECK (pue >= 1.0 AND pue <= 3.5),
    cue                      REAL NOT NULL CHECK (cue >= 0.5 AND cue <= 5.0),
    power_kw                 REAL NOT NULL CHECK (power_kw >= 0.0 AND power_kw <= 100000.0),
    cooling_kw               REAL NOT NULL CHECK (cooling_kw >= 0.0 AND cooling_kw <= 100000.0),
    thermal_output_kw        REAL NOT NULL CHECK (thermal_output_kw >= 0.0),
    throughput_qps           REAL NOT NULL CHECK (throughput_qps >= 0.0),
    joules_per_inference     REAL NOT NULL CHECK (joules_per_inference >= 0.0 AND joules_per_inference <= 1.0e6),
    eco_quota_kwh            REAL NOT NULL CHECK (eco_quota_kwh >= 0.0),
    eco_quota_window_start_utc TEXT NOT NULL,
    eco_quota_window_end_utc   TEXT NOT NULL,
    heat_governance_event_id   TEXT NOT NULL,
    ai_load_schedule_event_id  TEXT NOT NULL,
    r_energy_compute         REAL NOT NULL CHECK (r_energy_compute >= 0.0 AND r_energy_compute <= 1.0),
    r_cooling_water          REAL NOT NULL CHECK (r_cooling_water >= 0.0 AND r_cooling_water <= 1.0),
    r_carbon                 REAL NOT NULL CHECK (r_carbon >= 0.0 AND r_carbon <= 1.0),
    r_biodiversity           REAL NOT NULL CHECK (r_biodiversity >= 0.0 AND r_biodiversity <= 1.0),
    r_uncertainty            REAL NOT NULL CHECK (r_uncertainty >= 0.0 AND r_uncertainty <= 1.0),
    vt_before_ai             REAL NOT NULL CHECK (vt_before_ai >= 0.0),
    vt_after_ai              REAL NOT NULL CHECK (vt_after_ai >= 0.0),
    delta_vt_ai              REAL NOT NULL,
    k                        REAL NOT NULL CHECK (k >= 0.0 AND k <= 1.0),
    e                        REAL NOT NULL CHECK (e >= 0.0 AND e <= 1.0),
    r                        REAL NOT NULL CHECK (r >= 0.0 AND r <= 1.0),
    kerScore                 REAL NOT NULL CHECK (kerScore > 0.0),
    lane                     TEXT NOT NULL CHECK (lane IN ('RESEARCH','PILOT','PRODUCTION')),
    phoenix_hex_anchor       TEXT NOT NULL,
    prior_frame_id           TEXT
);

-- Audit logging tables for governance-AI coupling lifecycle.
CREATE TABLE IF NOT EXISTS audit_event_log (
    log_id              TEXT PRIMARY KEY,
    stage               TEXT NOT NULL, -- e.g., GovernanceEventGenerated, ConstraintCalculated
    event_id            TEXT NOT NULL,
    timestamp_utc       TEXT NOT NULL,
    payload_json        TEXT NOT NULL
);
