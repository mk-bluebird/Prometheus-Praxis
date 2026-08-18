PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS governance_identities (
    did TEXT PRIMARY KEY NOT NULL,
    display_name TEXT NOT NULL,
    role_name TEXT NOT NULL CHECK (role_name IN ('SOVEREIGN_MAINTAINER', 'AUTHORIZED_VALIDATOR')),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    created_at_utc TEXT NOT NULL,
    CHECK (
        did = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
        OR role_name = 'AUTHORIZED_VALIDATOR'
    )
);

INSERT OR IGNORE INTO governance_identities (
    did,
    display_name,
    role_name,
    active,
    created_at_utc
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'Prometheus-Praxis sovereign maintainer',
    'SOVEREIGN_MAINTAINER',
    1,
    '2026-08-17T00:00:00Z'
);

CREATE TABLE IF NOT EXISTS ker_band_policy (
    policy_id INTEGER PRIMARY KEY,
    policy_version TEXT NOT NULL UNIQUE,
    maintainer_did TEXT NOT NULL REFERENCES governance_identities(did),
    created_at_utc TEXT NOT NULL,
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor BETWEEN 0.0 AND 1.0),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value BETWEEN 0.0 AND 1.0),
    harm_risk REAL NOT NULL CHECK (harm_risk BETWEEN 0.0 AND 1.0),
    ker_score REAL NOT NULL CHECK (ker_score BETWEEN -1.0 AND 1.0),
    decision_band TEXT NOT NULL CHECK (decision_band IN ('SAFE', 'CAUTION', 'EXCLUDE')),
    policy_reason TEXT NOT NULL,
    CHECK (
        ABS(ker_score - knowledge_factor * (eco_impact_value - harm_risk)) <= 0.000001
    ),
    CHECK (
        (decision_band = 'SAFE' AND harm_risk >= 0.0 AND harm_risk <= 0.25)
        OR
        (decision_band = 'CAUTION' AND harm_risk > 0.25 AND harm_risk < 0.60)
        OR
        (decision_band = 'EXCLUDE' AND harm_risk >= 0.60 AND harm_risk <= 1.0)
    )
);

CREATE TABLE IF NOT EXISTS ker_policy_attestations (
    attestation_id INTEGER PRIMARY KEY,
    policy_id INTEGER NOT NULL REFERENCES ker_band_policy(policy_id) ON DELETE RESTRICT,
    maintainer_did TEXT NOT NULL REFERENCES governance_identities(did),
    attested_at_utc TEXT NOT NULL,
    attestation_note TEXT NOT NULL,
    UNIQUE (policy_id, maintainer_did)
);

CREATE INDEX IF NOT EXISTS idx_ker_policy_band_risk
    ON ker_band_policy(decision_band, harm_risk, policy_version);

CREATE INDEX IF NOT EXISTS idx_ker_policy_maintainer_time
    ON ker_band_policy(maintainer_did, created_at_utc DESC);

CREATE TRIGGER IF NOT EXISTS trg_ker_policy_insert_maintainer
BEFORE INSERT ON ker_band_policy
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.maintainer_did <> 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
        THEN RAISE(ABORT, 'policy maintainer must be the sovereign maintainer DID')
    END;
END;

CREATE TRIGGER IF NOT EXISTS trg_ker_policy_no_update
BEFORE UPDATE ON ker_band_policy
FOR EACH ROW
BEGIN
    SELECT RAISE(ABORT, 'KER policy records are append-only; create a superseding policy record');
END;

CREATE TRIGGER IF NOT EXISTS trg_ker_policy_no_delete
BEFORE DELETE ON ker_band_policy
FOR EACH ROW
BEGIN
    SELECT RAISE(ABORT, 'KER policy records are append-only and cannot be deleted');
END;

CREATE TRIGGER IF NOT EXISTS trg_ker_attestation_maintainer
BEFORE INSERT ON ker_policy_attestations
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.maintainer_did <> 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
        THEN RAISE(ABORT, 'only the sovereign maintainer DID may record this attestation')
    END;
END;

CREATE TRIGGER IF NOT EXISTS trg_ker_attestation_no_update
BEFORE UPDATE ON ker_policy_attestations
FOR EACH ROW
BEGIN
    SELECT RAISE(ABORT, 'attestations are append-only');
END;

CREATE TRIGGER IF NOT EXISTS trg_ker_attestation_no_delete
BEFORE DELETE ON ker_policy_attestations
FOR EACH ROW
BEGIN
    SELECT RAISE(ABORT, 'attestations cannot be deleted');
END;

CREATE VIEW IF NOT EXISTS v_ker_policy_band_status AS
SELECT
    policy.policy_id,
    policy.policy_version,
    policy.maintainer_did,
    policy.created_at_utc,
    policy.knowledge_factor,
    policy.eco_impact_value,
    policy.harm_risk,
    policy.ker_score,
    policy.decision_band,
    policy.policy_reason,
    COUNT(attestation.attestation_id) AS attestation_count
FROM ker_band_policy AS policy
LEFT JOIN ker_policy_attestations AS attestation
    ON attestation.policy_id = policy.policy_id
GROUP BY policy.policy_id;

WITH RECURSIVE risk_grid(risk_hundredths) AS (
    SELECT 0
    UNION ALL
    SELECT risk_hundredths + 1
    FROM risk_grid
    WHERE risk_hundredths < 100
),
band_equivalence AS (
    SELECT
        risk_hundredths / 100.0 AS harm_risk,
        CASE
            WHEN risk_hundredths / 100.0 <= 0.25 THEN 'SAFE'
            WHEN risk_hundredths / 100.0 < 0.60 THEN 'CAUTION'
            ELSE 'EXCLUDE'
        END AS sql_band,
        CASE
            WHEN risk_hundredths / 100.0 >= 0.0
             AND risk_hundredths / 100.0 <= 0.25 THEN 'SAFE'
            WHEN risk_hundredths / 100.0 > 0.25
             AND risk_hundredths / 100.0 < 0.60 THEN 'CAUTION'
            WHEN risk_hundredths / 100.0 >= 0.60
             AND risk_hundredths / 100.0 <= 1.0 THEN 'EXCLUDE'
            ELSE 'INVALID'
        END AS aln_band
    FROM risk_grid
)
SELECT
    harm_risk,
    sql_band,
    aln_band,
    CASE WHEN sql_band = aln_band THEN 'MATCH' ELSE 'MISMATCH' END AS equivalence
FROM band_equivalence
WHERE sql_band <> aln_band;
