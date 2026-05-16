-- filename: dbequation_ecoperjoule_sovereign_phoenix.sql
-- destination: ecorestorationshard/db/dbequation_ecoperjoule_sovereign_phoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- Example sovereignty validation aggregation view, if not present:
-- sovereign_energy_validation(nodeid, region, validation_count)

DROP VIEW IF EXISTS vagent_ecoperjoule_sovereign_phx;

CREATE VIEW vagent_ecoperjoule_sovereign_phx AS
SELECT
    v.nodeid,
    v.region,
    v.domain,
    v.twindowstart,
    v.twindowend,
    v.ecoperjoule             AS ecoperjoule_base,
    COALESCE(s.validation_count, 0) AS sovereign_validations,
    1.0 + log(1.0 + COALESCE(s.validation_count, 0)) AS sigma,
    v.ecoperjoule * (1.0 + log(1.0 + COALESCE(s.validation_count, 0))) AS ecoperjoule_sovereign
FROM vcyboquaticecoperjouleprodphx AS v
LEFT JOIN sovereign_energy_validation AS s
  ON s.nodeid = v.nodeid
 AND s.region = v.region
WHERE v.region = 'Phoenix-AZ';
