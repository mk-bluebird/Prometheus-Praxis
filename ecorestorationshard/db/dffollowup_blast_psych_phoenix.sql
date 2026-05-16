-- filename: dffollowup_blast_psych_phoenix.sql
-- destination: ecorestorationshard/db/dffollowup_blast_psych_phoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

DROP VIEW IF EXISTS vagent_blast_psych_phx;

CREATE VIEW vagent_blast_psych_phx AS
SELECT
    b.root_nodeid,
    b.nodeid,
    b.region,
    b.domain,
    COALESCE(m.neuroethic_radiushours, 0.0) AS neuroethic_radiushours,
    COALESCE(p.exposure_factor, 0.0)        AS exposure_factor,
    -- per-node contribution to affected brains
    1.0 * (COALESCE(m.neuroethic_radiushours, 0.0) / 24.0)
        * COALESCE(p.exposure_factor, 0.0) AS affected_brains_contrib
FROM vblastradiusrestorationphx AS b
LEFT JOIN vmt6883lanecontinuity AS m
  ON m.nodeid = b.nodeid
 AND m.region = b.region
LEFT JOIN psych_risk_exposure AS p
  ON p.nodeid = b.nodeid
 AND p.region = b.region
WHERE b.region = 'Phoenix-AZ';
