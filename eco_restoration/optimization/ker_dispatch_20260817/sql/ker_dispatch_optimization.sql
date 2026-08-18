PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS phoenix_canal_zones (
    canal_zone_id TEXT PRIMARY KEY NOT NULL,
    zone_name TEXT NOT NULL UNIQUE,
    hex_anchor_code TEXT NOT NULL,
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1))
);

CREATE TABLE IF NOT EXISTS ker_dispatch_candidates (
    dispatch_candidate_id TEXT PRIMARY KEY NOT NULL,
    canal_zone_id TEXT NOT NULL REFERENCES phoenix_canal_zones(canal_zone_id),
    activity_type TEXT NOT NULL CHECK (
        activity_type IN (
            'LOW_IMPACT_DREDGING',
            'NATIVE_PLANTING',
            'SWALE_MAINTENANCE',
            'BIOCHAR_MEDIA_REPLACEMENT',
            'SHADE_INSTALLATION'
        )
    ),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value BETWEEN 0.0 AND 1.0),
    harm_risk REAL NOT NULL CHECK (harm_risk BETWEEN 0.0 AND 1.0),
    energyreq_j INTEGER NOT NULL CHECK (energyreq_j BETWEEN 0 AND 1000000000),
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor BETWEEN 0.0 AND 1.0),
    ker_score REAL NOT NULL CHECK (ker_score BETWEEN -1.0 AND 1.0),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    CHECK (
        ABS(ker_score - knowledge_factor * (eco_impact_value - harm_risk)) <= 0.000001
    )
);

CREATE TABLE IF NOT EXISTS ker_dispatch_runs (
    ker_dispatch_run_id INTEGER PRIMARY KEY,
    run_label TEXT NOT NULL UNIQUE,
    harm_risk_threshold REAL NOT NULL CHECK (harm_risk_threshold BETWEEN 0.0 AND 1.0),
    energy_budget_j INTEGER NOT NULL CHECK (energy_budget_j BETWEEN 0 AND 1000000000),
    created_at_utc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS ker_dispatch_assignments (
    ker_dispatch_assignment_id INTEGER PRIMARY KEY,
    ker_dispatch_run_id INTEGER NOT NULL REFERENCES ker_dispatch_runs(ker_dispatch_run_id),
    dispatch_candidate_id TEXT NOT NULL REFERENCES ker_dispatch_candidates(dispatch_candidate_id),
    selected INTEGER NOT NULL CHECK (selected IN (0, 1)),
    decision_rank INTEGER NOT NULL CHECK (decision_rank >= 1),
    UNIQUE (ker_dispatch_run_id, dispatch_candidate_id),
    UNIQUE (ker_dispatch_run_id, decision_rank)
);

CREATE INDEX IF NOT EXISTS idx_dispatch_candidates_risk_energy_impact
    ON ker_dispatch_candidates(harm_risk, energyreq_j, eco_impact_value DESC, active);

CREATE INDEX IF NOT EXISTS idx_dispatch_candidate_zone_active
    ON ker_dispatch_candidates(canal_zone_id, active, activity_type);

CREATE INDEX IF NOT EXISTS idx_dispatch_assignment_run_rank
    ON ker_dispatch_assignments(ker_dispatch_run_id, selected, decision_rank);

CREATE TRIGGER IF NOT EXISTS trg_dispatch_assignment_constraints
BEFORE INSERT ON ker_dispatch_assignments
FOR EACH ROW
WHEN NEW.selected = 1
BEGIN
    SELECT CASE
        WHEN (
            SELECT candidate.harm_risk
            FROM ker_dispatch_candidates AS candidate
            WHERE candidate.dispatch_candidate_id = NEW.dispatch_candidate_id
        ) > (
            SELECT harm_risk_threshold
            FROM ker_dispatch_runs
            WHERE ker_dispatch_run_id = NEW.ker_dispatch_run_id
        )
        THEN RAISE(ABORT, 'selected dispatch candidate exceeds run harm-risk threshold')
        WHEN (
            SELECT COALESCE(SUM(candidate.energyreq_j), 0)
            FROM ker_dispatch_assignments AS assignment
            JOIN ker_dispatch_candidates AS candidate
                ON candidate.dispatch_candidate_id = assignment.dispatch_candidate_id
            WHERE assignment.ker_dispatch_run_id = NEW.ker_dispatch_run_id
              AND assignment.selected = 1
        ) + (
            SELECT energyreq_j
            FROM ker_dispatch_candidates
            WHERE dispatch_candidate_id = NEW.dispatch_candidate_id
        ) > (
            SELECT energy_budget_j
            FROM ker_dispatch_runs
            WHERE ker_dispatch_run_id = NEW.ker_dispatch_run_id
        )
        THEN RAISE(ABORT, 'selected dispatch candidates exceed energy budget')
    END;
END;

CREATE VIEW IF NOT EXISTS v_ker_dispatch_eligible_candidates AS
SELECT
    run.ker_dispatch_run_id,
    run.run_label,
    run.harm_risk_threshold,
    run.energy_budget_j,
    candidate.dispatch_candidate_id,
    candidate.canal_zone_id,
    candidate.activity_type,
    candidate.eco_impact_value,
    candidate.harm_risk,
    candidate.energyreq_j,
    candidate.knowledge_factor,
    candidate.ker_score
FROM ker_dispatch_runs AS run
JOIN ker_dispatch_candidates AS candidate
    ON candidate.active = 1
WHERE candidate.harm_risk <= run.harm_risk_threshold
  AND candidate.energyreq_j <= run.energy_budget_j;
