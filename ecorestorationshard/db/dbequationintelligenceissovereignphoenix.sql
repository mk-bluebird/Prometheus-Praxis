-- filename: dbequationintelligenceissovereignphoenix.sql
-- destination: ecorestorationshard/db/dbequationintelligenceissovereignphoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- EquationObject: equation.intelligence_is_sovereign.phoenix.2026v1
-- Ecoscope: NEURORIGHTS / BIOSCALE_FAIRNESS
-- Purpose: expose a boolean invariant per manifest indicating whether
--          the INTELLIGENCE_IS_SOVEREIGN clause is respected.

DROP VIEW IF EXISTS vagent_intelligence_is_sovereign_phx;

CREATE VIEW vagent_intelligence_is_sovereign_phx AS
SELECT
    m.manifestid,
    m.logicalname AS manifest_logicalname,
    m.region,
    m.ecoscope,
    m.laneband,
    m.status AS manifest_status,
    rib.bostromaddress,
    cis.intelligence_is_sovereign,
    cis.protected_stakeholder,
    cis.karma_floor,
    cis.bi_evidence_mode,
    cis.under_attack_state,
    cis.bi_mode_violation,
    cis.karma_floor_violation,
    cis.under_attack_freeze,
    CASE
        WHEN cis.intelligence_is_sovereign = 1
             AND (
                 cis.bi_mode_violation = 1
                 OR cis.karma_floor_violation = 1
                 OR cis.under_attack_freeze = 1
             )
        THEN 1
        ELSE 0
    END AS intelligence_is_sovereign_violation,
    CASE
        WHEN cis.intelligence_is_sovereign = 1
             AND (
                 cis.bi_mode_violation = 1
                 OR cis.karma_floor_violation = 1
                 OR cis.under_attack_freeze = 1
             )
        THEN 'AUTO_DENY_INTELLIGENCE_IS_SOVEREIGN'
        ELSE 'OK'
    END AS bioscale_fairness_decision
FROM manifestindex AS m
JOIN restorationidentitybinding AS rib
  ON rib.logicalname = m.logicalname
 AND rib.region = m.region
JOIN vbioscale_intelligence_is_sovereign AS cis
  ON cis.bostromaddress = rib.bostromaddress
WHERE m.region = 'Phoenix-AZ';
