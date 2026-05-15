-- filename: db_ecoperjoule_policy_energy.sql
-- destination: eco_restoration_shard/db/db_ecoperjoule_policy_energy.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 1. Eco-per-joule policy table for Phoenix energy planes

CREATE TABLE IF NOT EXISTS ecoperjoule_policy (
    policy_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    policy_name      TEXT NOT NULL,
    region           TEXT NOT NULL,
    lane             TEXT NOT NULL,   -- RESEARCH, EXPPROD, PROD
    domain           TEXT NOT NULL,   -- water, sewer, air, industrial, energy
    assetclass       TEXT NOT NULL,   -- e.g., CyboquaticMARNode, PumpStation
    theta_eco_min    REAL NOT NULL,   -- minimum eco-per-joule threshold
    k_min            REAL NOT NULL,   -- minimum K for lane
    e_min            REAL NOT NULL,   -- minimum E for lane
    r_max            REAL NOT NULL,   -- maximum R for lane
    active           INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    author_bostrom   TEXT NOT NULL,
    author_contractid TEXT NOT NULL,
    author_comment   TEXT,
    createdutc       TEXT NOT NULL,
    updatedutc       TEXT NOT NULL
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_ecoperjoule_policy_key
ON ecoperjoule_policy (
    region,
    lane,
    domain,
    assetclass
);

-- 2. CyboquaticEcoPlot extension view with ecoperjoule and carbonnegativeok

DROP VIEW IF EXISTS v_cyboquatic_ecoperjoule;

CREATE VIEW v_cyboquatic_ecoperjoule AS
SELECT
    c.nodeid,
    c.region,
    c.domain,
    c.twindowstart,
    c.twindowend,
    c.vtresidual,
    c.kscore,
    c.escore,
    c.rscore,
    c.lane,
    c.kerdeployable,
    c.esurplusjavg,
    c.pmarginkwavg,
    c.dEdtwavg,
    c.tailwinddutyfrac,
    c.karmadelta,
    CASE
        WHEN c.karmadelta IS NOT NULL
         AND (
             (c.esurplusjavg IS NOT NULL AND c.esurplusjavg > 0.0)
             OR (c.pmarginkwavg IS NOT NULL
                 AND c.twindowstart IS NOT NULL
                 AND c.twindowend   IS NOT NULL
             )
        )
        THEN
            c.karmadelta
            /
            CASE
                WHEN c.esurplusjavg IS NOT NULL AND c.esurplusjavg > 0.0
                    THEN c.esurplusjavg
                WHEN c.pmarginkwavg IS NOT NULL
                     AND c.twindowstart IS NOT NULL
                     AND c.twindowend   IS NOT NULL
                    THEN
                        c.pmarginkwavg * 1000.0 *
                        CAST(
                            (strftime('%s', c.twindowend) - strftime('%s', c.twindowstart))
                            AS REAL
                        )
                ELSE NULL
            END
        ELSE NULL
    END AS ecoperjoule
FROM CyboquaticEcoPlot AS c;

-- 3. PROD-only, policy-aware eco-per-joule view with carbonnegativeok

DROP VIEW IF EXISTS v_cyboquatic_ecoperjoule_prod_phx;

CREATE VIEW v_cyboquatic_ecoperjoule_prod_phx AS
SELECT
    v.nodeid,
    v.region,
    v.domain,
    v.twindowstart,
    v.twindowend,
    v.vtresidual,
    v.kscore,
    v.escore,
    v.rscore,
    v.lane,
    v.kerdeployable,
    v.esurplusjavg,
    v.pmarginkwavg,
    v.dEdtwavg,
    v.tailwinddutyfrac,
    v.karmadelta,
    v.ecoperjoule,
    p.theta_eco_min,
    p.k_min,
    p.e_min,
    p.r_max,
    CASE
        WHEN v.region        = p.region
         AND v.lane          = p.lane
         AND v.domain        = p.domain
         AND v.kscore       >= p.k_min
         AND v.escore       >= p.e_min
         AND v.rscore       <= p.r_max
         AND v.kerdeployable = 1
         AND v.ecoperjoule  IS NOT NULL
         AND v.ecoperjoule  >= p.theta_eco_min
        THEN 1
        ELSE 0
    END AS carbonnegativeok,
    p.author_bostrom,
    p.author_contractid,
    p.author_comment
FROM v_cyboquatic_ecoperjoule AS v
JOIN ecoperjoule_policy AS p
  ON p.region     = v.region
 AND p.lane       = v.lane
 AND p.domain     = v.domain
 AND p.assetclass = 'CyboquaticMARNode'
WHERE v.region = 'Phoenix-AZ'
  AND v.lane   = 'PROD'
  AND p.active = 1;
