PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS phoenix_hex_anchors (
    phoenix_hex_anchor_id INTEGER PRIMARY KEY,
    anchor_code TEXT NOT NULL UNIQUE,
    anchor_label TEXT NOT NULL,
    anchor_version TEXT NOT NULL,
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    created_at_utc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS canal_nodes (
    canal_node_id TEXT PRIMARY KEY NOT NULL,
    node_name TEXT NOT NULL UNIQUE,
    phoenix_hex_anchor_id INTEGER NOT NULL REFERENCES phoenix_hex_anchors(phoenix_hex_anchor_id),
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

CREATE TABLE IF NOT EXISTS surcharge_events (
    surcharge_event_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    observed_at_utc TEXT NOT NULL,
    discharge_m3_s REAL NOT NULL CHECK (discharge_m3_s > 0.0),
    duration_s REAL NOT NULL CHECK (duration_s > 0.0),
    energyreq_j REAL NOT NULL CHECK (energyreq_j BETWEEN 0.0 AND 1000000000.0),
    delta_vt REAL NOT NULL CHECK (delta_vt BETWEEN 0.0 AND 10000.0),
    UNIQUE (canal_node_id, observed_at_utc)
);

CREATE TABLE IF NOT EXISTS blast_radius_assessments (
    blast_radius_assessment_id INTEGER PRIMARY KEY,
    surcharge_event_id INTEGER NOT NULL UNIQUE REFERENCES surcharge_events(surcharge_event_id) ON DELETE CASCADE,
    conservative_radius_m REAL NOT NULL CHECK (conservative_radius_m > 0.0),
    assessed_zone TEXT NOT NULL CHECK (assessed_zone IN ('SAFE', 'CAUTION', 'EXCLUDE')),
    assessed_at_utc TEXT NOT NULL,
    ker_knowledge_factor REAL NOT NULL CHECK (ker_knowledge_factor BETWEEN 0.0 AND 1.0),
    ker_eco_impact_value REAL NOT NULL CHECK (ker_eco_impact_value BETWEEN 0.0 AND 1.0),
    ker_harm_risk REAL NOT NULL CHECK (ker_harm_risk BETWEEN 0.0 AND 1.0),
    ker_score REAL NOT NULL CHECK (ker_score BETWEEN -1.0 AND 1.0),
    CHECK (
        ABS(ker_score - ker_knowledge_factor * (ker_eco_impact_value - ker_harm_risk)) <= 0.000001
    ),
    CHECK (
        (assessed_zone = 'SAFE' AND ker_harm_risk <= 0.25)
        OR
        (assessed_zone = 'CAUTION' AND ker_harm_risk > 0.25 AND ker_harm_risk < 0.60)
        OR
        (assessed_zone = 'EXCLUDE' AND ker_harm_risk >= 0.60)
    )
);

CREATE TABLE IF NOT EXISTS node_exposure_zones (
    node_exposure_zone_id INTEGER PRIMARY KEY,
    blast_radius_assessment_id INTEGER NOT NULL REFERENCES blast_radius_assessments(blast_radius_assessment_id) ON DELETE CASCADE,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    measured_distance_m REAL NOT NULL CHECK (measured_distance_m >= 0.0),
    zone TEXT NOT NULL CHECK (zone IN ('SAFE', 'CAUTION', 'EXCLUDE')),
    routing_action TEXT NOT NULL CHECK (
        routing_action IN (
            'OPERATE_ONLY_AFTER_SITE_REVIEW',
            'HOLD_FOR_FIELD_INSPECTION',
            'NO_ENTRY_OR_OPERATION'
        )
    ),
    classified_at_utc TEXT NOT NULL,
    UNIQUE (blast_radius_assessment_id, canal_node_id),
    CHECK (
        (zone = 'SAFE' AND routing_action = 'OPERATE_ONLY_AFTER_SITE_REVIEW')
        OR
        (zone = 'CAUTION' AND routing_action = 'HOLD_FOR_FIELD_INSPECTION')
        OR
        (zone = 'EXCLUDE' AND routing_action = 'NO_ENTRY_OR_OPERATION')
    )
);

CREATE INDEX IF NOT EXISTS idx_radius_zone_covering
    ON blast_radius_assessments(
        assessed_zone,
        conservative_radius_m,
        surcharge_event_id,
        assessed_at_utc,
        ker_harm_risk
    );

CREATE INDEX IF NOT EXISTS idx_exposure_zone_covering
    ON node_exposure_zones(
        zone,
        blast_radius_assessment_id,
        canal_node_id,
        measured_distance_m,
        routing_action,
        classified_at_utc
    );

CREATE INDEX IF NOT EXISTS idx_surcharge_event_node_time_covering
    ON surcharge_events(
        canal_node_id,
        observed_at_utc DESC,
        surcharge_event_id,
        discharge_m3_s,
        duration_s
    );

CREATE INDEX IF NOT EXISTS idx_canal_node_hex_anchor_covering
    ON canal_nodes(
        phoenix_hex_anchor_id,
        active,
        canal_node_id,
        node_name,
        fog_media_state
    );

CREATE INDEX IF NOT EXISTS idx_hex_anchor_active_covering
    ON phoenix_hex_anchors(
        active,
        anchor_code,
        phoenix_hex_anchor_id,
        anchor_label,
        anchor_version
    );

CREATE TRIGGER IF NOT EXISTS trg_node_zone_matches_radius
BEFORE INSERT ON node_exposure_zones
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.zone = 'SAFE'
         AND NEW.measured_distance_m <= (
            SELECT conservative_radius_m
            FROM blast_radius_assessments
            WHERE blast_radius_assessment_id = NEW.blast_radius_assessment_id
         )
        THEN RAISE(ABORT, 'SAFE requires distance greater than conservative radius')
        WHEN NEW.zone = 'CAUTION'
         AND (
            NEW.measured_distance_m <= (
                SELECT conservative_radius_m / 2.0
                FROM blast_radius_assessments
                WHERE blast_radius_assessment_id = NEW.blast_radius_assessment_id
            )
            OR NEW.measured_distance_m > (
                SELECT conservative_radius_m
                FROM blast_radius_assessments
                WHERE blast_radius_assessment_id = NEW.blast_radius_assessment_id
            )
         )
        THEN RAISE(ABORT, 'CAUTION requires radius/2 < distance <= radius')
        WHEN NEW.zone = 'EXCLUDE'
         AND NEW.measured_distance_m > (
            SELECT conservative_radius_m / 2.0
            FROM blast_radius_assessments
            WHERE blast_radius_assessment_id = NEW.blast_radius_assessment_id
         )
        THEN RAISE(ABORT, 'EXCLUDE requires distance less than or equal to radius/2')
    END;
END;

CREATE VIEW IF NOT EXISTS v_hex_anchor_exposure_status AS
SELECT
    anchor.anchor_code,
    anchor.anchor_label,
    node.canal_node_id,
    node.node_name,
    event.observed_at_utc,
    assessment.conservative_radius_m,
    exposure.measured_distance_m,
    exposure.zone,
    exposure.routing_action,
    assessment.ker_knowledge_factor,
    assessment.ker_eco_impact_value,
    assessment.ker_harm_risk,
    assessment.ker_score
FROM node_exposure_zones AS exposure
JOIN blast_radius_assessments AS assessment
    ON assessment.blast_radius_assessment_id = exposure.blast_radius_assessment_id
JOIN surcharge_events AS event
    ON event.surcharge_event_id = assessment.surcharge_event_id
JOIN canal_nodes AS node
    ON node.canal_node_id = exposure.canal_node_id
JOIN phoenix_hex_anchors AS anchor
    ON anchor.phoenix_hex_anchor_id = node.phoenix_hex_anchor_id;

EXPLAIN QUERY PLAN
SELECT
    assessment.assessed_zone,
    assessment.conservative_radius_m,
    assessment.surcharge_event_id,
    assessment.assessed_at_utc,
    assessment.ker_harm_risk
FROM blast_radius_assessments AS assessment
WHERE assessment.assessed_zone = 'EXCLUDE'
  AND assessment.conservative_radius_m >= 50.0
ORDER BY assessment.conservative_radius_m;

EXPLAIN QUERY PLAN
SELECT
    exposure.zone,
    exposure.canal_node_id,
    exposure.measured_distance_m,
    exposure.routing_action,
    exposure.classified_at_utc
FROM node_exposure_zones AS exposure
WHERE exposure.zone IN ('CAUTION', 'EXCLUDE')
ORDER BY exposure.zone, exposure.classified_at_utc DESC;
