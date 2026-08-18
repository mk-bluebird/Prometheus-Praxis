PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS governance_identities (
    did TEXT PRIMARY KEY NOT NULL,
    role_name TEXT NOT NULL CHECK (
        role_name IN ('SOVEREIGN_MAINTAINER', 'AUTHORIZED_VALIDATOR', 'OBSERVATION_AGENT')
    ),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    created_at_utc TEXT NOT NULL,
    CHECK (
        did = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
        OR role_name IN ('AUTHORIZED_VALIDATOR', 'OBSERVATION_AGENT')
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

CREATE TABLE IF NOT EXISTS agent_trust_profiles (
    agent_id TEXT PRIMARY KEY NOT NULL,
    agent_did TEXT NOT NULL REFERENCES governance_identities(did),
    trust_weight REAL NOT NULL CHECK (trust_weight >= 0.0 AND trust_weight <= 1.0),
    evidence_scope TEXT NOT NULL,
    valid_from_utc TEXT NOT NULL,
    valid_to_utc TEXT,
    approved_by_did TEXT NOT NULL REFERENCES governance_identities(did),
    CHECK (valid_to_utc IS NULL OR valid_to_utc > valid_from_utc),
    CHECK (approved_by_did = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7')
);

CREATE TABLE IF NOT EXISTS ker_consensus_windows (
    consensus_window_id INTEGER PRIMARY KEY,
    window_label TEXT NOT NULL UNIQUE,
    starts_at_utc TEXT NOT NULL,
    ends_at_utc TEXT NOT NULL,
    maximum_permitted_disagreement REAL NOT NULL CHECK (
        maximum_permitted_disagreement BETWEEN 0.0 AND 1.0
    ),
    minimum_participating_agents INTEGER NOT NULL CHECK (
        minimum_participating_agents >= 1 AND minimum_participating_agents <= 10000
    ),
    created_by_did TEXT NOT NULL REFERENCES governance_identities(did),
    CHECK (ends_at_utc > starts_at_utc),
    CHECK (created_by_did = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7')
);

CREATE TABLE IF NOT EXISTS ker_agent_reports (
    ker_agent_report_id INTEGER PRIMARY KEY,
    consensus_window_id INTEGER NOT NULL REFERENCES ker_consensus_windows(consensus_window_id),
    agent_id TEXT NOT NULL REFERENCES agent_trust_profiles(agent_id),
    reported_at_utc TEXT NOT NULL,
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor BETWEEN 0.0 AND 1.0),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value BETWEEN 0.0 AND 1.0),
    harm_risk REAL NOT NULL CHECK (harm_risk BETWEEN 0.0 AND 1.0),
    evidence_note TEXT NOT NULL,
    UNIQUE (consensus_window_id, agent_id)
);

CREATE TABLE IF NOT EXISTS ker_consensus_results (
    ker_consensus_result_id INTEGER PRIMARY KEY,
    consensus_window_id INTEGER NOT NULL UNIQUE REFERENCES ker_consensus_windows(consensus_window_id),
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor BETWEEN 0.0 AND 1.0),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value BETWEEN 0.0 AND 1.0),
    harm_risk REAL NOT NULL CHECK (harm_risk BETWEEN 0.0 AND 1.0),
    disagreement_range REAL NOT NULL CHECK (disagreement_range BETWEEN 0.0 AND 1.0),
    participant_count INTEGER NOT NULL CHECK (participant_count >= 0),
    decision_band TEXT NOT NULL CHECK (decision_band IN ('SAFE', 'CAUTION', 'EXCLUDE')),
    result_status TEXT NOT NULL CHECK (
        result_status IN (
            'SAFE_SCREENING_ONLY',
            'CAUTION_REVIEW',
            'EXCLUDE',
            'HOLD_INSUFFICIENT_PARTICIPATION',
            'HOLD_CONFLICTING_INPUTS'
        )
    ),
    CHECK (
        (decision_band = 'SAFE' AND harm_risk <= 0.25)
        OR
        (decision_band = 'CAUTION' AND harm_risk > 0.25 AND harm_risk < 0.60)
        OR
        (decision_band = 'EXCLUDE' AND harm_risk >= 0.60)
    )
);

CREATE INDEX IF NOT EXISTS idx_agent_trust_active_window
    ON agent_trust_profiles(agent_id, trust_weight, valid_from_utc, valid_to_utc);

CREATE INDEX IF NOT EXISTS idx_ker_reports_window_agent
    ON ker_agent_reports(consensus_window_id, agent_id, harm_risk);

CREATE INDEX IF NOT EXISTS idx_ker_consensus_status_band
    ON ker_consensus_results(result_status, decision_band, harm_risk);

CREATE TRIGGER IF NOT EXISTS trg_agent_trust_sovereign_approval
BEFORE INSERT ON agent_trust_profiles
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.approved_by_did <> 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
        THEN RAISE(ABORT, 'agent trust profiles require sovereign approval')
    END;
END;

CREATE TRIGGER IF NOT EXISTS trg_report_active_agent
BEFORE INSERT ON ker_agent_reports
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NOT EXISTS (
            SELECT 1
            FROM agent_trust_profiles AS profile
            JOIN governance_identities AS identity_row
                ON identity_row.did = profile.agent_did
            WHERE profile.agent_id = NEW.agent_id
              AND identity_row.active = 1
              AND NEW.reported_at_utc >= profile.valid_from_utc
              AND (profile.valid_to_utc IS NULL OR NEW.reported_at_utc < profile.valid_to_utc)
        )
        THEN RAISE(ABORT, 'agent report lacks an active trust profile at report time')
    END;
END;

CREATE VIEW IF NOT EXISTS v_weighted_ker_consensus AS
WITH weighted AS (
    SELECT
        report.consensus_window_id,
        SUM(profile.trust_weight) AS total_weight,
        COUNT(*) AS participant_count,
        MIN(report.harm_risk) AS minimum_risk,
        MAX(report.harm_risk) AS maximum_risk,
        SUM(profile.trust_weight * report.knowledge_factor) AS weighted_k,
        SUM(profile.trust_weight * report.eco_impact_value) AS weighted_e,
        SUM(profile.trust_weight * report.harm_risk) AS weighted_r
    FROM ker_agent_reports AS report
    JOIN agent_trust_profiles AS profile
        ON profile.agent_id = report.agent_id
    GROUP BY report.consensus_window_id
)
SELECT
    window.consensus_window_id,
    window.window_label,
    weighted.participant_count,
    weighted.maximum_risk - weighted.minimum_risk AS disagreement_range,
    weighted.weighted_k / weighted.total_weight AS knowledge_factor,
    weighted.weighted_e / weighted.total_weight AS eco_impact_value,
    weighted.weighted_r / weighted.total_weight AS harm_risk,
    CASE
        WHEN weighted.weighted_r / weighted.total_weight <= 0.25 THEN 'SAFE'
        WHEN weighted.weighted_r / weighted.total_weight < 0.60 THEN 'CAUTION'
        ELSE 'EXCLUDE'
    END AS decision_band,
    CASE
        WHEN weighted.participant_count < window.minimum_participating_agents
            THEN 'HOLD_INSUFFICIENT_PARTICIPATION'
        WHEN weighted.maximum_risk - weighted.minimum_risk > window.maximum_permitted_disagreement
            THEN 'HOLD_CONFLICTING_INPUTS'
        WHEN weighted.weighted_r / weighted.total_weight >= 0.60
            THEN 'EXCLUDE'
        WHEN weighted.weighted_r / weighted.total_weight > 0.25
            THEN 'CAUTION_REVIEW'
        ELSE 'SAFE_SCREENING_ONLY'
    END AS result_status
FROM ker_consensus_windows AS window
JOIN weighted
    ON weighted.consensus_window_id = window.consensus_window_id
WHERE weighted.total_weight > 0.0;
