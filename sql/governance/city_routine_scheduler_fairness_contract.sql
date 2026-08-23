PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS scheduler_fairness_policy (
    policy_id TEXT PRIMARY KEY NOT NULL,
    active INTEGER NOT NULL CHECK (active IN (0, 1)),
    minimum_equity_floor REAL NOT NULL
        CHECK (minimum_equity_floor >= 0.0 AND minimum_equity_floor <= 1.0),
    maximum_accessibility_risk REAL NOT NULL
        CHECK (maximum_accessibility_risk >= 0.0 AND maximum_accessibility_risk <= 1.0),
    minimum_knowledge_factor REAL NOT NULL
        CHECK (minimum_knowledge_factor >= 0.0 AND minimum_knowledge_factor <= 1.0),
    baseline_service_reduction_prohibited INTEGER NOT NULL
        CHECK (baseline_service_reduction_prohibited = 1),
    cybernetic_host_needs_must_not_be_excluded INTEGER NOT NULL
        CHECK (cybernetic_host_needs_must_not_be_excluded = 1),
    created_at_epoch_seconds INTEGER NOT NULL CHECK (created_at_epoch_seconds >= 0)
) STRICT;

CREATE TABLE IF NOT EXISTS resident_declared_schedule_constraint (
    constraint_receipt_id TEXT PRIMARY KEY NOT NULL,
    service_id TEXT NOT NULL,
    constraint_kind TEXT NOT NULL CHECK (
        constraint_kind IN (
            'mobility',
            'language',
            'sensory',
            'temperature_sensitivity',
            'cybernetic_device_need'
        )
    ),
    constraint_value TEXT NOT NULL,
    purpose TEXT NOT NULL,
    consent_active INTEGER NOT NULL CHECK (consent_active IN (0, 1)),
    revocation_available INTEGER NOT NULL CHECK (revocation_available IN (0, 1)),
    expires_at_epoch_seconds INTEGER NOT NULL CHECK (expires_at_epoch_seconds >= 0),
    baseline_service_available_without_declaration INTEGER NOT NULL
        CHECK (baseline_service_available_without_declaration = 1),
    identity_inference_used INTEGER NOT NULL CHECK (identity_inference_used = 0),
    created_at_epoch_seconds INTEGER NOT NULL CHECK (created_at_epoch_seconds >= 0)
) STRICT;

CREATE TABLE IF NOT EXISTS city_routine_schedule_candidate (
    candidate_id TEXT PRIMARY KEY NOT NULL,
    policy_id TEXT NOT NULL,
    service_id TEXT NOT NULL,
    area_id TEXT NOT NULL,
    scheduled_at_epoch_seconds INTEGER NOT NULL CHECK (scheduled_at_epoch_seconds >= 0),
    energy_joules REAL NOT NULL CHECK (energy_joules >= 0.0),
    knowledge_factor REAL NOT NULL
        CHECK (knowledge_factor >= 0.0 AND knowledge_factor <= 1.0),
    eco_impact_value REAL NOT NULL
        CHECK (eco_impact_value >= 0.0 AND eco_impact_value <= 1.0),
    accessibility_risk REAL NOT NULL
        CHECK (accessibility_risk >= 0.0 AND accessibility_risk <= 1.0),
    equity_floor_score REAL NOT NULL
        CHECK (equity_floor_score >= 0.0 AND equity_floor_score <= 1.0),
    baseline_service_preserved INTEGER NOT NULL
        CHECK (baseline_service_preserved IN (0, 1)),
    supports_declared_cybernetic_host_need INTEGER NOT NULL
        CHECK (supports_declared_cybernetic_host_need IN (0, 1)),
    schedule_status TEXT NOT NULL
        CHECK (schedule_status IN ('PROPOSED', 'VALIDATED', 'REJECTED')),
    created_at_epoch_seconds INTEGER NOT NULL CHECK (created_at_epoch_seconds >= 0),
    FOREIGN KEY (policy_id) REFERENCES scheduler_fairness_policy(policy_id)
) STRICT;

CREATE INDEX IF NOT EXISTS city_routine_schedule_candidate_policy_status_index
ON city_routine_schedule_candidate (policy_id, schedule_status, scheduled_at_epoch_seconds);

CREATE INDEX IF NOT EXISTS resident_declared_constraint_service_expiry_index
ON resident_declared_schedule_constraint (
    service_id,
    consent_active,
    expires_at_epoch_seconds
);

CREATE TRIGGER IF NOT EXISTS city_routine_schedule_reject_invalid_candidate
BEFORE INSERT ON city_routine_schedule_candidate
FOR EACH ROW
WHEN NEW.schedule_status = 'VALIDATED'
 AND EXISTS (
    SELECT 1
    FROM scheduler_fairness_policy AS policy
    WHERE policy.policy_id = NEW.policy_id
      AND policy.active = 1
      AND (
            NEW.knowledge_factor < policy.minimum_knowledge_factor
         OR NEW.accessibility_risk > policy.maximum_accessibility_risk
         OR NEW.equity_floor_score < policy.minimum_equity_floor
         OR NEW.baseline_service_preserved <> 1
         OR (
                policy.cybernetic_host_needs_must_not_be_excluded = 1
            AND NEW.supports_declared_cybernetic_host_need <> 1
            )
      )
 )
BEGIN
    SELECT RAISE(
        ABORT,
        'schedule validation rejected: fairness, accessibility, evidence, or baseline-service invariant failed'
    );
END;

CREATE TRIGGER IF NOT EXISTS city_routine_schedule_reject_inactive_policy
BEFORE INSERT ON city_routine_schedule_candidate
FOR EACH ROW
WHEN NEW.schedule_status = 'VALIDATED'
 AND NOT EXISTS (
    SELECT 1
    FROM scheduler_fairness_policy AS policy
    WHERE policy.policy_id = NEW.policy_id
      AND policy.active = 1
 )
BEGIN
    SELECT RAISE(
        ABORT,
        'schedule validation rejected: active fairness policy is required'
    );
END;

CREATE VIEW IF NOT EXISTS active_resident_declared_schedule_constraints AS
SELECT
    constraint_receipt_id,
    service_id,
    constraint_kind,
    constraint_value,
    purpose,
    expires_at_epoch_seconds
FROM resident_declared_schedule_constraint
WHERE consent_active = 1
  AND revocation_available = 1
  AND identity_inference_used = 0;
