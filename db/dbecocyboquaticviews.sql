-- filename: dbecocyboquaticviews.sql
-- destination: Eco-Fort/db/dbecocyboquaticviews.sql

PRAGMA foreign_keys = ON;

-- DR-CYBO-ECO-VIEW-001
-- CyboquaticEcoPlot ecoper-joule views (non-actuating, governance-only)

DROP VIEW IF EXISTS vcyboquaticecoperjoule;

CREATE VIEW vcyboquaticecoperjoule AS
SELECT
    -- identity and governance scope
    nodeid,
    region,
    domain,
    lane,
    kerdeployable,
    twindowstart,
    twindowend,

    -- KER and residual, as already carried by CyboquaticEcoPlot
    vtresidual,
    kscore,
    escore,
    rscore,

    -- existing energy / eco metrics (columns already present or planned)
    esurplusjavg,     -- average surplus energy over window, joules if present
    esurplusjmin,
    esurplusjmax,
    pmarginkwavg,     -- average power margin in kW if present
    dEdtwavg,         -- average dE/dt over window
    tailwinddutyfrac, -- fraction of time in tailwind mode

    contaminant,      -- contaminant bundle label
    mremovedkg,       -- mass removed over window
    ecoimpactraw,     -- raw eco impact before trust discount
    ecoimpactadj,     -- trust-adjusted eco impact
    karmadelta,       -- governance-window karma delta

    -- window duration in seconds (derived from ISO-8601 timestamps)
    CAST(strftime('%s', twindowend) - strftime('%s', twindowstart) AS REAL)
        AS windowseconds,

    -- canonical energy over window, in joules.
    -- priority order:
    --   1) esurplusjavg if already recorded in joules,
    --   2) pmarginkwavg * windowseconds * 1000 (kW * s * 1000 = J),
    --   3) NULL if neither pathway applies.
    CASE
        WHEN esurplusjavg IS NOT NULL THEN
            esurplusjavg
        WHEN pmarginkwavg IS NOT NULL
             AND twindowstart IS NOT NULL
             AND twindowend   IS NOT NULL THEN
            pmarginkwavg
            * CAST(strftime('%s', twindowend) - strftime('%s', twindowstart) AS REAL)
            * 1000.0
        ELSE
            NULL
    END AS energyjoules,

    -- eco-per-joule ratio: karmadelta / energyjoules, when both are present
    -- and energyjoules > 0. This is the scalar used for carbon-negative gating.
    CASE
        WHEN karmadelta IS NOT NULL
             AND (
                    esurplusjavg IS NOT NULL
                 OR (pmarginkwavg IS NOT NULL
                     AND twindowstart IS NOT NULL
                     AND twindowend   IS NOT NULL)
                 )
             AND (
                    CASE
                        WHEN esurplusjavg IS NOT NULL THEN
                            esurplusjavg
                        WHEN pmarginkwavg IS NOT NULL
                             AND twindowstart IS NOT NULL
                             AND twindowend   IS NOT NULL THEN
                            pmarginkwavg
                            * CAST(strftime('%s', twindowend) - strftime('%s', twindowstart) AS REAL)
                            * 1000.0
                        ELSE
                            NULL
                    END
                 ) > 0.0
        THEN
            karmadelta
            / (
                CASE
                    WHEN esurplusjavg IS NOT NULL THEN
                        esurplusjavg
                    WHEN pmarginkwavg IS NOT NULL
                         AND twindowstart IS NOT NULL
                         AND twindowend   IS NOT NULL THEN
                        pmarginkwavg
                        * CAST(strftime('%s', twindowend) - strftime('%s', twindowstart) AS REAL)
                        * 1000.0
                    ELSE
                        NULL
                END
              )
        ELSE
            NULL
    END AS ecoperjoule
FROM
    CyboquaticEcoPlot;


-- Optional governance-focused PROD view:
-- filters to PROD lane, kerdeployable = 1, and non-null ecoperjoule.
-- This can be referenced by lane views and schedulers as a canonical
-- carbon-negative candidate surface.

DROP VIEW IF EXISTS vcyboquaticecoperjoule_prod;

CREATE VIEW vcyboquaticecoperjoule_prod AS
SELECT
    *
FROM
    vcyboquaticecoperjoule
WHERE
    lane = 'PROD'
    AND kerdeployable = 1
    AND ecoperjoule IS NOT NULL;
