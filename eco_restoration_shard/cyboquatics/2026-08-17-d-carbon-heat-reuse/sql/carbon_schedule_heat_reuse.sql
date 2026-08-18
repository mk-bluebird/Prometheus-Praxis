PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS canal_nodes (
    canal_node_id TEXT PRIMARY KEY NOT NULL,
    node_name TEXT NOT NULL UNIQUE,
    fog_media_state TEXT NOT NULL CHECK (
        fog_media_state IN (
            'AQUEOUS_MODELED',
            'FIELD_SCREENED_NOT_ALL_FOG_CONDITIONS_MET',
            'UNMODELED_MIXED_OIL_WATER_SEDIMENT',
            'UNKNOWN_MEDIA'
        )
    ),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1))
);

CREATE TABLE IF NOT EXISTS ker_assessments (
    ker_assessment_id INTEGER PRIMARY KEY,
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor BETWEEN 0.0 AND 1.0),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value BETWEEN 0.0 AND 1.0),
    harm_risk REAL NOT NULL CHECK (harm_risk BETWEEN 0.0 AND 1.0),
    ker_score REAL NOT NULL CHECK (ker_score BETWEEN -1.0 AND 1.0),
    CHECK (ABS(ker_score - knowledge_factor * (eco_impact_value - harm_risk)) <= 0.000001)
);

CREATE TABLE IF NOT EXISTS energy_budget_periods (
    energy_budget_period_id INTEGER PRIMARY KEY,
    period_label TEXT NOT NULL UNIQUE,
    starts_at_utc TEXT NOT NULL,
    ends_at_utc TEXT NOT NULL,
    energy_budget_j REAL NOT NULL CHECK (energy_budget_j >= 0.0),
    CHECK (ends_at_utc > starts_at_utc)
);

CREATE TABLE IF NOT EXISTS restoration_workloads (
    restoration_workload_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    workload_label TEXT NOT NULL UNIQUE,
    activity_type TEXT NOT NULL CHECK (
        activity_type IN ('LOW_IMPACT_DREDGING', 'NATIVE_PLANTING', 'WASTE_DIVERSION', 'HABITAT_MAINTENANCE')
    ),
    energyreq_j REAL NOT NULL CHECK (energyreq_j > 0.0),
    expected_sequestered_kg_co2e REAL NOT NULL CHECK (expected_sequestered_kg_co2e >= 0.0),
    expected_emitted_kg_co2e REAL NOT NULL CHECK (expected_emitted_kg_co2e >= 0.0),
    net_carbon_kg_co2e REAL GENERATED ALWAYS AS (
        expected_sequestered_kg_co2e - expected_emitted_kg_co2e
    ) STORED,
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id)
);

CREATE TABLE IF NOT EXISTS workload_schedule_assignments (
    workload_schedule_assignment_id INTEGER PRIMARY KEY,
    energy_budget_period_id INTEGER NOT NULL REFERENCES energy_budget_periods(energy_budget_period_id),
    restoration_workload_id INTEGER NOT NULL REFERENCES restoration_workloads(restoration_workload_id),
    scheduled INTEGER NOT NULL CHECK (scheduled IN (0, 1)),
    decision_rank INTEGER NOT NULL CHECK (decision_rank >= 1),
    assigned_at_utc TEXT NOT NULL,
    UNIQUE (energy_budget_period_id, restoration_workload_id),
    UNIQUE (energy_budget_period_id, decision_rank)
);

CREATE TABLE IF NOT EXISTS heat_reuse_sinks (
    heat_reuse_sink_id INTEGER PRIMARY KEY,
    canal_node_id TEXT REFERENCES canal_nodes(canal_node_id),
    sink_name TEXT NOT NULL UNIQUE,
    sink_type TEXT NOT NULL CHECK (sink_type IN ('GREENHOUSE', 'COMPOST', 'CANAL_WATER')),
    target_max_temperature_c REAL NOT NULL CHECK (target_max_temperature_c >= -20.0 AND target_max_temperature_c <= 100.0),
    canal_connection INTEGER NOT NULL CHECK (canal_connection IN (0, 1)),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    CHECK (
        (sink_type = 'CANAL_WATER' AND canal_connection = 1)
        OR
        (sink_type IN ('GREENHOUSE', 'COMPOST') AND canal_connection = 0)
    )
);

CREATE TABLE IF NOT EXISTS ai_heat_telemetry (
    ai_heat_telemetry_id INTEGER PRIMARY KEY,
    observed_at_utc TEXT NOT NULL,
    source_label TEXT NOT NULL,
    ai_energy_j REAL NOT NULL CHECK (ai_energy_j >= 0.0),
    heat_recovery_efficiency REAL NOT NULL CHECK (
        heat_recovery_efficiency >= 0.0 AND heat_recovery_efficiency <= 1.0
    ),
    reusable_heat_j REAL GENERATED ALWAYS AS (
        ai_energy_j * heat_recovery_efficiency
    ) STORED,
    energy_carbon_kg_co2e REAL NOT NULL CHECK (energy_carbon_kg_co2e >= 0.0)
);

CREATE TABLE IF NOT EXISTS heat_reuse_allocations (
    heat_reuse_allocation_id INTEGER PRIMARY KEY,
    ai_heat_telemetry_id INTEGER NOT NULL REFERENCES ai_heat_telemetry(ai_heat_telemetry_id),
    heat_reuse_sink_id INTEGER NOT NULL REFERENCES heat_reuse_sinks(heat_reuse_sink_id),
    heat_demand_j REAL NOT NULL CHECK (heat_demand_j >= 0.0),
    allocated_heat_j REAL NOT NULL CHECK (allocated_heat_j >= 0.0),
    current_temperature_c REAL NOT NULL,
    status TEXT NOT NULL CHECK (
        status IN (
            'ELIGIBLE_FOR_SITE_THERMAL_REVIEW',
            'HOLD_OVER_TARGET_TEMPERATURE',
            'REJECT_CANAL_WATER_WARMING'
        )
    ),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id),
    CHECK (allocated_heat_j <= heat_demand_j)
);

CREATE INDEX IF NOT EXISTS idx_restoration_workloads_net_carbon_energy
    ON restoration_workloads(net_carbon_kg_co2e DESC, energyreq_j ASC, workload_label);

CREATE INDEX IF NOT EXISTS idx_schedule_period_rank
    ON workload_schedule_assignments(energy_budget_period_id, decision_rank, scheduled);

CREATE INDEX IF NOT EXISTS idx_heat_telemetry_time
    ON ai_heat_telemetry(observed_at_utc DESC, reusable_heat_j);

CREATE INDEX IF NOT EXISTS idx_heat_allocations_sink_status
    ON heat_reuse_allocations(heat_reuse_sink_id, status, ai_heat_telemetry_id);

CREATE TRIGGER IF NOT EXISTS trg_schedule_positive_carbon_only
BEFORE INSERT ON workload_schedule_assignments
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.scheduled = 1 AND (
            SELECT net_carbon_kg_co2e
            FROM restoration_workloads
            WHERE restoration_workload_id = NEW.restoration_workload_id
        ) <= 0.0
        THEN RAISE(ABORT, 'scheduled workload must have positive net carbon benefit')
    END;

    SELECT CASE
        WHEN NEW.scheduled = 1 AND (
            SELECT COALESCE(SUM(workload.energyreq_j), 0.0)
            FROM workload_schedule_assignments AS assignment
            JOIN restoration_workloads AS workload
                ON workload.restoration_workload_id = assignment.restoration_workload_id
            WHERE assignment.energy_budget_period_id = NEW.energy_budget_period_id
              AND assignment.scheduled = 1
        ) + (
            SELECT energyreq_j
            FROM restoration_workloads
            WHERE restoration_workload_id = NEW.restoration_workload_id
        ) > (
            SELECT energy_budget_j
            FROM energy_budget_periods
            WHERE energy_budget_period_id = NEW.energy_budget_period_id
        )
        THEN RAISE(ABORT, 'scheduled energy demand exceeds the period energy budget')
    END;
END;

CREATE TRIGGER IF NOT EXISTS trg_heat_reuse_safety
BEFORE INSERT ON heat_reuse_allocations
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.allocated_heat_j > (
            SELECT reusable_heat_j
            FROM ai_heat_telemetry
            WHERE ai_heat_telemetry_id = NEW.ai_heat_telemetry_id
        )
        THEN RAISE(ABORT, 'allocated heat exceeds available recovered heat')
        WHEN (
            SELECT sink_type
            FROM heat_reuse_sinks
            WHERE heat_reuse_sink_id = NEW.heat_reuse_sink_id
        ) = 'CANAL_WATER'
        THEN RAISE(ABORT, 'canal-water warming is excluded from automatic heat reuse')
        WHEN NEW.current_temperature_c > (
            SELECT target_max_temperature_c
            FROM heat_reuse_sinks
            WHERE heat_reuse_sink_id = NEW.heat_reuse_sink_id
        )
        THEN RAISE(ABORT, 'heat sink is above its approved temperature limit')
    END;
END;

CREATE VIEW IF NOT EXISTS v_carbon_negative_schedule_candidates AS
SELECT
    workload.restoration_workload_id,
    workload.workload_label,
    workload.activity_type,
    workload.energyreq_j,
    workload.expected_sequestered_kg_co2e,
    workload.expected_emitted_kg_co2e,
    workload.net_carbon_kg_co2e,
    workload.net_carbon_kg_co2e / workload.energyreq_j AS net_carbon_per_joule,
    ker.knowledge_factor,
    ker.eco_impact_value,
    ker.harm_risk,
    ker.ker_score
FROM restoration_workloads AS workload
JOIN ker_assessments AS ker
    ON ker.ker_assessment_id = workload.ker_assessment_id
WHERE workload.net_carbon_kg_co2e > 0.0
ORDER BY net_carbon_per_joule DESC, workload.net_carbon_kg_co2e DESC, workload.workload_label ASC;

CREATE VIEW IF NOT EXISTS v_heat_reuse_status AS
SELECT
    telemetry.observed_at_utc,
    telemetry.source_label,
    telemetry.ai_energy_j,
    telemetry.heat_recovery_efficiency,
    telemetry.reusable_heat_j,
    sink.sink_name,
    sink.sink_type,
    allocation.heat_demand_j,
    allocation.allocated_heat_j,
    allocation.current_temperature_c,
    allocation.status,
    ker.knowledge_factor,
    ker.eco_impact_value,
    ker.harm_risk,
    ker.ker_score
FROM heat_reuse_allocations AS allocation
JOIN ai_heat_telemetry AS telemetry
    ON telemetry.ai_heat_telemetry_id = allocation.ai_heat_telemetry_id
JOIN heat_reuse_sinks AS sink
    ON sink.heat_reuse_sink_id = allocation.heat_reuse_sink_id
JOIN ker_assessments AS ker
    ON ker.ker_assessment_id = allocation.ker_assessment_id;
