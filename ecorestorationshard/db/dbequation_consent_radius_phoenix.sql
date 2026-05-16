-- filename: dbequation_consent_radius_phoenix.sql
-- destination: ecorestorationshard/db/dbequation_consent_radius_phoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

DROP VIEW IF EXISTS vagent_consent_radius_phx;

CREATE VIEW vagent_consent_radius_phx AS
SELECT
    c.nodeid,
    c.region,
    c.base_neuroethic_radius_hours AS base_radius_hours,
    COALESCE(t.consent_count, 0)   AS consent_count,
    -- example consent_factor = min(0.5, 0.05 * consent_count)
    MIN(0.5, 0.05 * COALESCE(t.consent_count, 0)) AS consent_factor,
    c.base_neuroethic_radius_hours
        * (1.0 + MIN(0.5, 0.05 * COALESCE(t.consent_count, 0))) AS neuroethic_radius_allowed_hours
FROM vmt6883lanecontinuity AS c
LEFT JOIN mt6883_consent_tx AS t
  ON t.nodeid         = c.nodeid
 AND t.region         = c.region
 AND t.bostromaddress = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
WHERE c.region = 'Phoenix-AZ';
