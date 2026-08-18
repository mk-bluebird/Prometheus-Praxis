PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS governance_identities (
    did TEXT PRIMARY KEY NOT NULL,
    role_name TEXT NOT NULL CHECK (
        role_name IN ('SOVEREIGN_MAINTAINER', 'AUTHORIZED_VALIDATOR')
    ),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    created_at_utc TEXT NOT NULL,
    CHECK (
        did = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
        OR role_name = 'AUTHORIZED_VALIDATOR'
    )
);

INSERT OR IGNORE INTO governance_identities (
    did,
    role_name,
    active,
    created_at_utc
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'SOVEREIGN_MAINTAINER',
    1,
    '2026-08-17T00:00:00Z'
);

CREATE TABLE IF NOT EXISTS ker_decisions (
    ker_decision_id INTEGER PRIMARY KEY,
    decision_label TEXT NOT NULL UNIQUE,
    decided_at_utc TEXT NOT NULL,
    decided_by_did TEXT NOT NULL REFERENCES governance_identities(did),
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor BETWEEN 0.0 AND 1.0),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value BETWEEN 0.0 AND 1.0),
    harm_risk REAL NOT NULL CHECK (harm_risk BETWEEN 0.0 AND 1.0),
    ker_score REAL NOT NULL CHECK (ker_score BETWEEN -1.0 AND 1.0),
    decision_band TEXT NOT NULL CHECK (decision_band IN ('SAFE', 'CAUTION', 'EXCLUDE')),
    decision_note TEXT NOT NULL,
    CHECK (
        ABS(ker_score - knowledge_factor * (eco_impact_value - harm_risk)) <= 0.000001
    ),
    CHECK (
        (decision_band = 'SAFE' AND harm_risk <= 0.25)
        OR
        (decision_band = 'CAUTION' AND harm_risk > 0.25 AND harm_risk < 0.60)
        OR
        (decision_band = 'EXCLUDE' AND harm_risk >= 0.60)
    )
);

CREATE TABLE IF NOT EXISTS ker_decision_audit (
    ker_decision_audit_id INTEGER PRIMARY KEY,
    ker_decision_id INTEGER NOT NULL REFERENCES ker_decisions(ker_decision_id) ON DELETE RESTRICT,
    event_kind TEXT NOT NULL CHECK (event_kind IN ('CREATE', 'UPDATE')),
    event_at_utc TEXT NOT NULL,
    actor_did TEXT NOT NULL REFERENCES governance_identities(did),
    old_knowledge_factor REAL,
    new_knowledge_factor REAL NOT NULL CHECK (new_knowledge_factor BETWEEN 0.0 AND 1.0),
    delta_knowledge_factor REAL NOT NULL,
    old_eco_impact_value REAL,
    new_eco_impact_value REAL NOT NULL CHECK (new_eco_impact_value BETWEEN 0.0 AND 1.0),
    delta_eco_impact_value REAL NOT NULL,
    old_harm_risk REAL,
    new_harm_risk REAL NOT NULL CHECK (new_harm_risk BETWEEN 0.0 AND 1.0),
    delta_harm_risk REAL NOT NULL,
    old_ker_score REAL,
    new_ker_score REAL NOT NULL CHECK (new_ker_score BETWEEN -1.0 AND 1.0),
    delta_ker_score REAL NOT NULL,
    old_decision_band TEXT,
    new_decision_band TEXT NOT NULL CHECK (new_decision_band IN ('SAFE', 'CAUTION', 'EXCLUDE')),
    event_note TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS canal_nodes (
    canal_node_id TEXT PRIMARY KEY NOT NULL,
    node_name TEXT NOT NULL UNIQUE,
    bank_sensitivity REAL NOT NULL CHECK (bank_sensitivity BETWEEN 0.0 AND 1.0),
    critical_shear_stress_pa REAL NOT NULL CHECK (
        critical_shear_stress_pa >= 0.0 AND critical_shear_stress_pa <= 1000000.0
    ),
    sediment_transport_potential REAL NOT NULL CHECK (
        sediment_transport_potential BETWEEN 0.0 AND 1.0
    ),
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

CREATE TABLE IF NOT EXISTS surcharge_breach_events (
    surcharge_breach_event_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    observed_at_utc TEXT NOT NULL,
    discharge_m3_s REAL NOT NULL CHECK (discharge_m3_s > 0.0 AND discharge_m3_s <= 100000.0),
    duration_s REAL NOT NULL CHECK (duration_s > 0.0 AND duration_s <= 604800.0),
    energyreq_j REAL NOT NULL CHECK (energyreq_j >= 0.0 AND energyreq_j <= 1000000000.0),
    delta_vt REAL NOT NULL CHECK (delta_vt >= 0.0 AND delta_vt <= 10000.0),
    ker_decision_id INTEGER NOT NULL REFERENCES ker_decisions(ker_decision_id),
    UNIQUE (canal_node_id, observed_at_utc)
);

CREATE TABLE IF NOT EXISTS blast_radius_coefficients (
    coefficient_set_id INTEGER PRIMARY KEY,
    coefficient_label TEXT NOT NULL UNIQUE,
    c_m_per_sqrt_m3 REAL NOT NULL CHECK (c_m_per_sqrt_m3 > 0.0),
    alpha_bank_sensitivity REAL NOT NULL CHECK (
        alpha_bank_sensitivity >= 0.0 AND alpha_bank_sensitivity <= 1000.0
    ),
    beta_m_per_pa REAL NOT NULL CHECK (beta_m_per_pa >= 0.0 AND beta_m_per_pa <= 1000.0),
    calibrated_by_did TEXT NOT NULL REFERENCES governance_identities(did),
    calibrated_at_utc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS blast_radius_assessments (
    blast_radius_assessment_id INTEGER PRIMARY KEY,
    surcharge_breach_event_id INTEGER NOT NULL UNIQUE REFERENCES surcharge_breach_events(surcharge_breach_event_id) ON DELETE CASCADE,
    coefficient_set_id INTEGER NOT NULL REFERENCES blast_radius_coefficients(coefficient_set_id),
    base_radius_m REAL NOT NULL CHECK (base_radius_m >= 0.0),
    bank_adjusted_radius_m REAL NOT NULL CHECK (bank_adjusted_radius_m >= base_radius_m),
    sediment_radius_component_m REAL NOT NULL CHECK (sediment_radius_component_m >= 0.0),
    conservative_radius_m REAL NOT NULL CHECK (conservative_radius_m >= bank_adjusted_radius_m),
    risk_zone TEXT NOT NULL CHECK (risk_zone IN ('SAFE', 'CAUTION', 'EXCLUDE')),
    CHECK (
        ABS(conservative_radius_m - (bank_adjusted_radius_m + sediment_radius_component_m)) <= 0.000001
    )
);

CREATE INDEX IF NOT EXISTS idx_ker_decisions_band_risk
    ON ker_decisions(decision_band, harm_risk, decided_at_utc DESC);

CREATE INDEX IF NOT EXISTS idx_ker_audit_decision_time
    ON ker_decision_audit(ker_decision_id, event_at_utc DESC, event_kind);

CREATE INDEX IF NOT EXISTS idx_surcharge_event_node_time
    ON surcharge_breach_events(canal_node_id, observed_at_utc DESC);

CREATE INDEX IF NOT EXISTS idx_radius_assessment_zone_radius
    ON blast_radius_assessments(risk_zone, conservative_radius_m);

CREATE TRIGGER IF NOT EXISTS trg_ker_decision_create_audit
AFTER INSERT ON ker_decisions
FOR EACH ROW
BEGIN
    INSERT INTO ker_decision_audit (
        ker_decision_id,
        event_kind,
        event_at_utc,
        actor_did,
        old_knowledge_factor,
        new_knowledge_factor,
        delta_knowledge_factor,
        old_eco_impact_value,
        new_eco_impact_value,
        delta_eco_impact_value,
        old_harm_risk,
        new_harm_risk,
        delta_harm_risk,
        old_ker_score,
        new_ker_score,
        delta_ker_score,
        old_decision_band,
        new_decision_band,
        event_note
    ) VALUES (
        NEW.ker_decision_id,
        'CREATE',
        NEW.decided_at_utc,
        NEW.decided_by_did,
        NULL,
        NEW.knowledge_factor,
        NEW.knowledge_factor,
        NULL,
        NEW.eco_impact_value,
        NEW.eco_impact_value,
        NULL,
        NEW.harm_risk,
        NEW.harm_risk,
        NULL,
        NEW.ker_score,
        NEW.ker_score,
        NULL,
        NEW.decision_band,
        NEW.decision_note
    );
END;

CREATE TRIGGER IF NOT EXISTS trg_ker_decision_update_audit
AFTER UPDATE ON ker_decisions
FOR EACH ROW
WHEN
    NEW.knowledge_factor <> OLD.knowledge_factor
    OR NEW.eco_impact_value <> OLD.eco_impact_value
    OR NEW.harm_risk <> OLD.harm_risk
    OR NEW.ker_score <> OLD.ker_score
    OR NEW.decision_band <> OLD.decision_band
    OR NEW.decision_note <> OLD.decision_note
BEGIN
    INSERT INTO ker_decision_audit (
        ker_decision_id,
        event_kind,
        event_at_utc,
        actor_did,
        old_knowledge_factor,
        new_knowledge_factor,
        delta_knowledge_factor,
        old_eco_impact_value,
        new_eco_impact_value,
        delta_eco_impact_value,
        old_harm_risk,
        new_harm_risk,
        delta_harm_risk,
        old_ker_score,
        new_ker_score,
        delta_ker_score,
        old_decision_band,
        new_decision_band,
        event_note
    ) VALUES (
        NEW.ker_decision_id,
        'UPDATE',
        NEW.decided_at_utc,
        NEW.decided_by_did,
        OLD.knowledge_factor,
        NEW.knowledge_factor,
        NEW.knowledge_factor - OLD.knowledge_factor,
        OLD.eco_impact_value,
        NEW.eco_impact_value,
        NEW.eco_impact_value - OLD.eco_impact_value,
        OLD.harm_risk,
        NEW.harm_risk,
        NEW.harm_risk - OLD.harm_risk,
        OLD.ker_score,
        NEW.ker_score,
        NEW.ker_score - OLD.ker_score,
        OLD.decision_band,
        NEW.decision_band,
        NEW.decision_note
    );
END;

CREATE TRIGGER IF NOT EXISTS trg_ker_decision_no_delete
BEFORE DELETE ON ker_decisions
FOR EACH ROW
BEGIN
    SELECT RAISE(ABORT, 'KER decisions are append-only and cannot be deleted');
END;

CREATE TRIGGER IF NOT EXISTS trg_ker_audit_no_update
BEFORE UPDATE ON ker_decision_audit
FOR EACH ROW
BEGIN
    SELECT RAISE(ABORT, 'KER audit rows are immutable');
END;

CREATE TRIGGER IF NOT EXISTS trg_ker_audit_no_delete
BEFORE DELETE ON ker_decision_audit
FOR EACH ROW
BEGIN
    SELECT RAISE(ABORT, 'KER audit rows cannot be deleted');
END;

CREATE VIEW IF NOT EXISTS v_ker_audit_history AS
SELECT
    audit.ker_decision_audit_id,
    decision.decision_label,
    audit.event_kind,
    audit.event_at_utc,
    audit.actor_did,
    audit.old_knowledge_factor,
    audit.new_knowledge_factor,
    audit.delta_knowledge_factor,
    audit.old_eco_impact_value,
    audit.new_eco_impact_value,
    audit.delta_eco_impact_value,
    audit.old_harm_risk,
    audit.new_harm_risk,
    audit.delta_harm_risk,
    audit.old_ker_score,
    audit.new_ker_score,
    audit.delta_ker_score,
    audit.old_decision_band,
    audit.new_decision_band,
    audit.event_note
FROM ker_decision_audit AS audit
JOIN ker_decisions AS decision
    ON decision.ker_decision_id = audit.ker_decision_id;

CREATE VIEW IF NOT EXISTS v_sediment_blast_radius_inputs AS
SELECT
    event.surcharge_breach_event_id,
    node.canal_node_id,
    node.node_name,
    event.observed_at_utc,
    event.discharge_m3_s,
    event.duration_s,
    node.bank_sensitivity,
    node.critical_shear_stress_pa,
    node.sediment_transport_potential,
    coefficient.c_m_per_sqrt_m3,
    coefficient.alpha_bank_sensitivity,
    coefficient.beta_m_per_pa,
    coefficient.c_m_per_sqrt_m3 * SQRT(event.discharge_m3_s * event.duration_s) AS base_radius_m,
    coefficient.c_m_per_sqrt_m3
        * SQRT(event.discharge_m3_s * event.duration_s)
        * (1.0 + coefficient.alpha_bank_sensitivity * node.bank_sensitivity) AS bank_adjusted_radius_m,
    coefficient.beta_m_per_pa * node.critical_shear_stress_pa AS sediment_radius_component_m,
    coefficient.c_m_per_sqrt_m3
        * SQRT(event.discharge_m3_s * event.duration_s)
        * (1.0 + coefficient.alpha_bank_sensitivity * node.bank_sensitivity)
        + coefficient.beta_m_per_pa * node.critical_shear_stress_pa AS conservative_radius_m
FROM surcharge_breach_events AS event
JOIN canal_nodes AS node ON node.canal_node_id = event.canal_node_id
JOIN blast_radius_assessments AS assessment
    ON assessment.surcharge_breach_event_id = event.surcharge_breach_event_id
JOIN blast_radius_coefficients AS coefficient
    ON coefficient.coefficient_set_id = assessment.coefficient_set_id;
