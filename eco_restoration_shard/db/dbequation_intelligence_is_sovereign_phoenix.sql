-- filename dbequation_intelligence_is_sovereign_phoenix.sql
-- destination eco_restoration_shard/db/dbequation_intelligence_is_sovereign_phoenix.sql
-- repo-target github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-----------------------------------------------------------------------
-- EquationObject: equation.intelligence_is_sovereign.phoenix.2026v1
-- Ecoscope: NEURORIGHTS / BIOSCALEFAIRNESS
-- Purpose: expose per-identity invariant violations for the
--          INTELLIGENCE_IS_SOVEREIGN clause in Phoenix.
-----------------------------------------------------------------------

-- Identity + sovereign state snapshot.
DROP VIEW IF EXISTS vbioscale_intelligence_is_sovereign;

CREATE VIEW vbioscale_intelligence_is_sovereign AS
WITH identitystate AS (
    SELECT
        i.bostromaddress,
        i.region,
        i.currentkarma AS kcurrent,
        s.intelligenceissovereign,
        s.protectedstakeholder,
        s.karmafloor,
        s.bievidencemode,
        s.underattackstate
    FROM identitykarma AS i
    JOIN cyberaugcitizensovereign AS s
      ON s.bostromaddress = i.bostromaddress
     AND s.region         = i.region
),
biinvariant AS (
    -- BI evidence mode invariance:
    -- if clause is active, evidence mode must remain HASHONLY or REDACTED.
    SELECT
        isv.bostromaddress,
        CASE
            WHEN isv.intelligenceissovereign = 1
             AND isv.bievidencemode NOT IN ('HASHONLY','REDACTED')
            THEN 1 ELSE 0
        END AS bimodeviolation
    FROM identitystate AS isv
),
karmainvariant AS (
    -- Karma floor invariance:
    -- if clause + protectedstakeholder active, K must not fall below karmafloor.
    SELECT
        isv.bostromaddress,
        CASE
            WHEN isv.intelligenceissovereign = 1
             AND isv.protectedstakeholder    = 1
             AND isv.kcurrent < isv.karmafloor
            THEN 1 ELSE 0
        END AS karmafloorviolation
    FROM identitystate AS isv
),
underattackinvariant AS (
    -- UnderAttack freeze:
    -- if UnderAttack, downgrade attempts must be frozen; here we just flag.
    SELECT
        isv.bostromaddress,
        CASE
            WHEN isv.intelligenceissovereign = 1
             AND isv.underattackstate = 'UNDERATTACK'
            THEN 1 ELSE 0
        END AS underattackfreeze
    FROM identitystate AS isv
),
aggregate AS (
    SELECT
        isv.bostromaddress,
        isv.region,
        isv.intelligenceissovereign,
        isv.protectedstakeholder,
        isv.kcurrent,
        isv.karmafloor,
        isv.bievidencemode,
        isv.underattackstate,
        bi.bimodeviolation,
        kv.karmafloorviolation,
        ua.underattackfreeze,
        (bi.bimodeviolation +
         kv.karmafloorviolation +
         ua.underattackfreeze) AS violationcount
    FROM identitystate AS isv
    JOIN biinvariant        AS bi ON bi.bostromaddress = isv.bostromaddress
    JOIN karmainvariant     AS kv ON kv.bostromaddress = isv.bostromaddress
    JOIN underattackinvariant AS ua ON ua.bostromaddress = isv.bostromaddress
)
SELECT
    a.bostromaddress,
    a.region,
    a.intelligenceissovereign,
    a.protectedstakeholder,
    a.kcurrent,
    a.karmafloor,
    a.bievidencemode,
    a.underattackstate,
    a.bimodeviolation,
    a.karmafloorviolation,
    a.underattackfreeze,
    CASE
        WHEN a.intelligenceissovereign = 1
         AND a.violationcount > 0
        THEN 1
        ELSE 0
    END AS mustautodeny
FROM aggregate AS a;
