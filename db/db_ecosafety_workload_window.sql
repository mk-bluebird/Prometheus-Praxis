-- Filename: db/db_ecosafety_workload_window.sql
-- Role: Ecosafety workload node window ledger for Prometheus-Praxis,
--       aligned with ecosafety.workload.node.window.v1 and WorkloadNodeWindow.

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS ecosafety_workload_node_window (
    -- Primary identity for the window row.
    window_id            INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Shard identity and object binding (matches ShardRowBase).
    shardid              TEXT    NOT NULL,
    timestamputc         INTEGER NOT NULL,  -- window close UTC seconds
    objectid             TEXT    NOT NULL,

    -- Node / asset identifiers.
    node_id              TEXT    NOT NULL,
    asset_id             TEXT    NULL,

    -- Window bounds.
    window_start_utc     INTEGER NOT NULL,
    window_end_utc       INTEGER NOT NULL,

    -- Energy totals (Joules).
    energy_req_j         REAL    NOT NULL,
    energy_surplus_j     REAL    NOT NULL,

    -- Workload fractions (0..1).
    accepted_fraction    REAL    NOT NULL,
    rejected_fraction    REAL    NOT NULL,
    rerouted_fraction    REAL    NOT NULL,

    -- Lyapunov residual traces.
    mean_vt_before       REAL    NOT NULL,
    mean_vt_after        REAL    NOT NULL,
    mean_delta_vt        REAL    NOT NULL,

    -- Risk scalars (0..1), optional.
    mean_r_carbon        REAL    NULL,
    mean_r_biodiv        REAL    NULL,

    -- Corridor semantics and decision mode.
    corridor_status      TEXT    NOT NULL,  -- SATISFIED | SOFTVIOLATION | HARDVIOLATION
    decision_mode        TEXT    NOT NULL,  -- ALLOW | DERATE | STOP

    -- Window-level KER (stored inline for fast queries).
    ker_k                REAL    NOT NULL,
    ker_e                REAL    NOT NULL,
    ker_r                REAL    NOT NULL
);

-- Basic check constraints for invariants.
ALTER TABLE ecosafety_workload_node_window
    ADD CONSTRAINT chk_workload_time_order
    CHECK (window_end_utc >= window_start_utc);

ALTER TABLE ecosafety_workload_node_window
    ADD CONSTRAINT chk_workload_fractions_bounds
    CHECK (
        accepted_fraction BETWEEN 0.0 AND 1.0 AND
        rejected_fraction BETWEEN 0.0 AND 1.0 AND
        rerouted_fraction BETWEEN 0.0 AND 1.0
    );

ALTER TABLE ecosafety_workload_node_window
    ADD CONSTRAINT chk_workload_corridor_status
    CHECK (
        corridor_status IN ('SATISFIED','SOFTVIOLATION','HARDVIOLATION')
    );

ALTER TABLE ecosafety_workload_node_window
    ADD CONSTRAINT chk_workload_decision_mode
    CHECK (
        decision_mode IN ('ALLOW','DERATE','STOP')
    );

COMMIT;
