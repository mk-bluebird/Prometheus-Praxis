-- filename: dbequation_kpis_phoenix.sql
-- destination: ecorestorationshard/db/dbequation_kpis_phoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- Assumptions:
--  - vagentrestorationnodesphx exposes: nodeid (or scopeid), region, kscore, escore, rscore.
--  - psych_risk_exposure view exposes: nodeid, region, psych_risk_flag (0 or 1).

DROP VIEW IF EXISTS vagent_kpis_phoenix;

CREATE VIEW vagent_kpis_phoenix AS
SELECT
    n.region,
    n.domain,
    n.scopeid      AS nodeid,
    n.kscore,
    n.escore,
    n.rscore,
    COALESCE(p.psych_risk_flag, 0) AS psych_risk_flag,
    0.001                           AS epsilon,
    CASE
        WHEN COALESCE(p.psych_risk_flag, 0) = 1 THEN 0.0
        WHEN n.rscore IS NULL OR n.kscore IS NULL OR n.escore IS NULL THEN NULL
        ELSE (n.kscore * n.escore) / (n.rscore + 0.001)
    END AS kpis
FROM vagentrestorationnodesphx AS n
LEFT JOIN psych_risk_exposure AS p
  ON p.nodeid = n.scopeid
 AND p.region = n.region
WHERE n.region = 'Phoenix-AZ';
