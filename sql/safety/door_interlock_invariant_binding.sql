PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS door_interlock_event (
    event_id TEXT PRIMARY KEY NOT NULL,
    door_id TEXT NOT NULL,
    observed_at_ms INTEGER NOT NULL CHECK (observed_at_ms >= 0),
    obstacle_detected INTEGER NOT NULL CHECK (obstacle_detected IN (0, 1)),
    forced_signal_detected INTEGER NOT NULL CHECK (forced_signal_detected IN (0, 1)),
    resistance_newtons REAL NOT NULL CHECK (resistance_newtons >= 0.0),
    resistance_threshold_newtons REAL NOT NULL CHECK (resistance_threshold_newtons >= 0.0),
    sensor_evidence_sufficient INTEGER NOT NULL CHECK (sensor_evidence_sufficient IN (0, 1)),
    requested_motion TEXT NOT NULL CHECK (requested_motion IN ('OPEN', 'HOLD_OPEN', 'REVERSE', 'CLOSE')),
    no_biometric_classification_used INTEGER NOT NULL CHECK (no_biometric_classification_used = 1),
    equal_body_type_safety_required INTEGER NOT NULL CHECK (equal_body_type_safety_required = 1),
    augmented_limb_or_device_protection_required INTEGER NOT NULL
        CHECK (augmented_limb_or_device_protection_required = 1),
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor >= 0.0 AND knowledge_factor <= 1.0),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value >= 0.0 AND eco_impact_value <= 1.0),
    harm_risk REAL NOT NULL CHECK (harm_risk >= 0.0 AND harm_risk <= 1.0),
    created_at_epoch_seconds INTEGER NOT NULL CHECK (created_at_epoch_seconds >= 0)
) STRICT;

CREATE TABLE IF NOT EXISTS door_interlock_audit (
    audit_id INTEGER PRIMARY KEY,
    event_id TEXT NOT NULL,
    door_id TEXT NOT NULL,
    observed_at_ms INTEGER NOT NULL,
    requested_motion TEXT NOT NULL,
    safe_close INTEGER NOT NULL CHECK (safe_close IN (0, 1)),
    outcome TEXT NOT NULL CHECK (outcome IN ('ACCEPTED_CLOSE', 'RECORDED_NON_CLOSE')),
    created_at_epoch_seconds INTEGER NOT NULL CHECK (created_at_epoch_seconds >= 0),
    FOREIGN KEY (event_id) REFERENCES door_interlock_event(event_id)
) STRICT;

CREATE INDEX IF NOT EXISTS door_interlock_event_door_time_index
ON door_interlock_event (door_id, observed_at_ms DESC);

CREATE INDEX IF NOT EXISTS door_interlock_event_motion_index
ON door_interlock_event (requested_motion, observed_at_ms DESC);

CREATE TRIGGER IF NOT EXISTS door_interlock_reject_unsafe_close_insert
BEFORE INSERT ON door_interlock_event
FOR EACH ROW
WHEN NEW.requested_motion = 'CLOSE'
 AND (
        NEW.obstacle_detected <> 0
     OR NEW.forced_signal_detected <> 0
     OR NEW.resistance_newtons > NEW.resistance_threshold_newtons
     OR NEW.sensor_evidence_sufficient <> 1
 )
BEGIN
    SELECT RAISE(
        ABORT,
        'door close rejected: obstacle, forced signal, resistance, or sensor evidence invariant failed'
    );
END;

CREATE TRIGGER IF NOT EXISTS door_interlock_reject_unsafe_close_update
BEFORE UPDATE OF
    obstacle_detected,
    forced_signal_detected,
    resistance_newtons,
    resistance_threshold_newtons,
    sensor_evidence_sufficient,
    requested_motion
ON door_interlock_event
FOR EACH ROW
WHEN NEW.requested_motion = 'CLOSE'
 AND (
        NEW.obstacle_detected <> 0
     OR NEW.forced_signal_detected <> 0
     OR NEW.resistance_newtons > NEW.resistance_threshold_newtons
     OR NEW.sensor_evidence_sufficient <> 1
 )
BEGIN
    SELECT RAISE(
        ABORT,
        'door close rejected: obstacle, forced signal, resistance, or sensor evidence invariant failed'
    );
END;

CREATE TRIGGER IF NOT EXISTS door_interlock_record_safe_close
AFTER INSERT ON door_interlock_event
FOR EACH ROW
WHEN NEW.requested_motion = 'CLOSE'
 AND NEW.obstacle_detected = 0
 AND NEW.forced_signal_detected = 0
 AND NEW.resistance_newtons <= NEW.resistance_threshold_newtons
 AND NEW.sensor_evidence_sufficient = 1
BEGIN
    INSERT INTO door_interlock_audit (
        event_id,
        door_id,
        observed_at_ms,
        requested_motion,
        safe_close,
        outcome,
        created_at_epoch_seconds
    ) VALUES (
        NEW.event_id,
        NEW.door_id,
        NEW.observed_at_ms,
        NEW.requested_motion,
        1,
        'ACCEPTED_CLOSE',
        NEW.created_at_epoch_seconds
    );
END;

CREATE TRIGGER IF NOT EXISTS door_interlock_record_non_close
AFTER INSERT ON door_interlock_event
FOR EACH ROW
WHEN NEW.requested_motion <> 'CLOSE'
BEGIN
    INSERT INTO door_interlock_audit (
        event_id,
        door_id,
        observed_at_ms,
        requested_motion,
        safe_close,
        outcome,
        created_at_epoch_seconds
    ) VALUES (
        NEW.event_id,
        NEW.door_id,
        NEW.observed_at_ms,
        NEW.requested_motion,
        0,
        'RECORDED_NON_CLOSE',
        NEW.created_at_epoch_seconds
    );
END;
