-- File: sql/cyboquatic_workload_schema.sql

PRAGMA foreign_keys = ON;

-- Canal nodes with KER parameters and DID binding.
CREATE TABLE IF NOT EXISTS canal_node (
    canal_node_id TEXT PRIMARY KEY,
    hex_id        TEXT NOT NULL,
    owner_did     TEXT NOT NULL,
    ker_K_min     REAL NOT NULL CHECK (ker_K_min >= 0.0 AND ker_K_min <= 1.0),
    ker_E_min     REAL NOT NULL CHECK (ker_E_min >= 0.0 AND ker_E_min <= 1.0),
    ker_R_max     REAL NOT NULL CHECK (ker_R_max >= 0.0 AND ker_R_max <= 1.0),
    created_utc   TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc   TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    CHECK (owner_did = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7')
);

-- Workload cycles telemetry: energyreqJ, ΔVt, K,E,R.
CREATE TABLE IF NOT EXISTS workload_cycle (
    cycle_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    hex_id          TEXT NOT NULL,
    canal_node_id   TEXT NOT NULL REFERENCES canal_node(canal_node_id) ON DELETE CASCADE,
    energyreqJ      REAL NOT NULL CHECK (energyreqJ >= 0.0),
    risk_h_before   REAL NOT NULL CHECK (risk_h_before >= 0.0 AND risk_h_before <= 1.0),
    risk_e_before   REAL NOT NULL CHECK (risk_e_before >= 0.0 AND risk_e_before <= 1.0),
    risk_t_before   REAL NOT NULL CHECK (risk_t_before >= 0.0 AND risk_t_before <= 1.0),
    risk_b_before   REAL NOT NULL CHECK (risk_b_before >= 0.0 AND risk_b_before <= 1.0),
    risk_h_after    REAL NOT NULL CHECK (risk_h_after >= 0.0 AND risk_h_after <= 1.0),
    risk_e_after    REAL NOT NULL CHECK (risk_e_after >= 0.0 AND risk_e_after <= 1.0),
    risk_t_after    REAL NOT NULL CHECK (risk_t_after >= 0.0 AND risk_t_after <= 1.0),
    risk_b_after    REAL NOT NULL CHECK (risk_b_after >= 0.0 AND risk_b_after <= 1.0),
    delta_Vt        REAL NOT NULL,
    K               REAL NOT NULL CHECK (K >= 0.0 AND K <= 1.0),
    E               REAL NOT NULL CHECK (E >= 0.0 AND E <= 1.0),
    R               REAL NOT NULL CHECK (R >= 0.0 AND R <= 1.0),
    created_utc     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    CHECK (delta_Vt <= 0.5),    -- Corridor: limit Lyapunov increase.
    CHECK (energyreqJ <= 1000000.0) -- Upper bound for single-cycle energy.
);

-- KER aggregation over windows (e.g., daily, monsoon shard).
CREATE TABLE IF NOT EXISTS ker_window (
    window_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    hex_id        TEXT NOT NULL,
    canal_node_id TEXT NOT NULL REFERENCES canal_node(canal_node_id) ON DELETE CASCADE,
    window_label  TEXT NOT NULL,
    K_avg         REAL NOT NULL CHECK (K_avg >= 0.0 AND K_avg <= 1.0),
    E_avg         REAL NOT NULL CHECK (E_avg >= 0.0 AND E_avg <= 1.0),
    R_max         REAL NOT NULL CHECK (R_max >= 0.0 AND R_max <= 1.0),
    created_utc   TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc   TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

-- FOG and unmodeled media telemetry.
CREATE TABLE IF NOT EXISTS fog_flow (
    fog_id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    hex_id                 TEXT NOT NULL,
    canal_node_id          TEXT NOT NULL REFERENCES canal_node(canal_node_id) ON DELETE CASCADE,
    fog_concentration_mgL  REAL NOT NULL CHECK (fog_concentration_mgL >= 0.0),
    unmodeled_media_flag   REAL NOT NULL CHECK (unmodeled_media_flag >= 0.0 AND unmodeled_media_flag <= 1.0),
    flow_rate_Lps          REAL NOT NULL CHECK (flow_rate_Lps >= 0.0),
    created_utc            TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

-- Trigger: enforce KER corridor per workload_cycle relative to canal_node.
CREATE TRIGGER IF NOT EXISTS trg_workload_ker_check
AFTER INSERT ON workload_cycle
BEGIN
    UPDATE workload_cycle
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE cycle_id = NEW.cycle_id;

    -- Abort if K,E,R outside canal_node corridor.
    SELECT
        CASE
            WHEN NEW.K < (SELECT ker_K_min FROM canal_node WHERE canal_node_id = NEW.canal_node_id)
                OR NEW.E < (SELECT ker_E_min FROM canal_node WHERE canal_node_id = NEW.canal_node_id)
                OR NEW.R > (SELECT ker_R_max FROM canal_node WHERE canal_node_id = NEW.canal_node_id)
            THEN RAISE(ABORT, 'KER corridor violation for workload_cycle')
        END;
END;

-- View: blast-radius style workload exceedance per hex and canal node.
CREATE VIEW IF NOT EXISTS v_workload_exceedance AS
SELECT
    hex_id,
    canal_node_id,
    COUNT(*) AS total_cycles,
    SUM(CASE WHEN R > 0.7 THEN 1 ELSE 0 END) AS high_R_cycles,
    SUM(CASE WHEN energyreqJ > 50000.0 THEN 1 ELSE 0 END) AS high_energy_cycles
FROM workload_cycle
GROUP BY hex_id, canal_node_id;
