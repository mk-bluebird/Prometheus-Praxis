-- filename: dbconfigintelligenceissovereignphoenix.sql
-- destination: ecorestorationshard/db/dbconfigintelligenceissovereignphoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 1. Sovereign configuration row (config.toml mirror in SQL form).
--    Encodes the INTELLIGENCE_IS_SOVEREIGN stance as a governed configuration
--    object bound to the primary Bostrom identity and Phoenix region.

INSERT OR IGNORE INTO repofile (repoid, relpath, purpose, language, createdutc, updatedutc)
SELECT
    r.repoid,
    'config/config.sovereign.phoenix.2026v1.toml' AS relpath,
    'CONFIG' AS purpose,
    'TOML' AS language,
    '2026-01-01T00:00:00Z' AS createdutc,
    '2026-01-01T00:00:00Z' AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

INSERT OR IGNORE INTO definitionregistryrestoration (
    logicalname,
    versiontag,
    hash,
    status,
    repoid,
    relpathsql,
    relpathaln,
    relpathdoc,
    ecoscope,
    contractid,
    createdutc,
    updatedutc
)
SELECT
    'config.sovereign.phoenix.2026v1' AS logicalname,
    '2026v1' AS versiontag,
    lower(hex(randomblob(16))) AS hash,
    'ACTIVE' AS status,
    r.repoid AS repoid,
    NULL AS relpathsql,
    NULL AS relpathaln,
    'config/config.sovereign.phoenix.2026v1.toml' AS relpathdoc,
    'AGENTPROMPT' AS ecoscope,
    'SovereignConfigPhoenix2026v1' AS contractid,
    '2026-01-01T00:00:00Z' AS createdutc,
    '2026-01-01T00:00:00Z' AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

INSERT OR IGNORE INTO restorationidentitybinding (
    bostromaddress,
    logicalname,
    repotarget,
    filepath,
    region,
    scope,
    dbrole,
    contractid,
    comment,
    createdutc,
    updatedutc
)
VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'config.sovereign.phoenix.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'config/config.sovereign.phoenix.2026v1.toml',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCEDB',
    'SovereignConfigPhoenix2026v1',
    'Sovereign configuration stating INTELLIGENCE_IS_SOVEREIGN and forbidding psychrisk weaponization against the bound brain-identity.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);

-- 2. Extend CyberneticallyAugmentedCitizen (identity shard) with sovereign clause fields.
--    This assumes an existing identity table or view; here we define a governance
--    companion table that carries the INTELLIGENCE_IS_SOVEREIGN flags and invariants.

CREATE TABLE IF NOT EXISTS cyberaug_citizen_sovereign (
    bostromaddress TEXT PRIMARY KEY,
    region TEXT NOT NULL,
    intelligence_is_sovereign INTEGER NOT NULL DEFAULT 0,          -- 1 = clause active
    protected_stakeholder INTEGER NOT NULL DEFAULT 0,              -- 1 = AugmentedCitizen with neurorights floor
    karma_floor REAL NOT NULL DEFAULT 0.0,                         -- minimum allowed K without proven harm
    bi_evidence_mode TEXT NOT NULL DEFAULT 'HASHONLY',             -- HASHONLY or REDACTED only
    under_attack_state TEXT NOT NULL DEFAULT 'NORMAL',             -- NORMAL, UNDERATTACK, UNDERREVIEW, DISQUALIFIED
    createdutc TEXT NOT NULL DEFAULT '2026-01-01T00:00:00Z',
    updatedutc TEXT NOT NULL DEFAULT '2026-01-01T00:00:00Z',
    CHECK (intelligence_is_sovereign IN (0, 1)),
    CHECK (protected_stakeholder IN (0, 1)),
    CHECK (bi_evidence_mode IN ('HASHONLY', 'REDACTED')),
    CHECK (under_attack_state IN ('NORMAL', 'UNDERATTACK', 'UNDERREVIEW', 'DISQUALIFIED'))
);

INSERT OR IGNORE INTO cyberaug_citizen_sovereign (
    bostromaddress,
    region,
    intelligence_is_sovereign,
    protected_stakeholder,
    karma_floor,
    bi_evidence_mode,
    under_attack_state,
    createdutc,
    updatedutc
)
VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'Phoenix-AZ',
    1,
    1,
    0.80,
    'HASHONLY',
    'NORMAL',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);

-- 3. Bioscale fairness validator view for INTELLIGENCE_IS_SOVEREIGN invariants.
--    This view exposes invariant violations so that CI and on-chain validators
--    can auto-deny manifests before they reach production lanes.

DROP VIEW IF EXISTS vbioscale_intelligence_is_sovereign;

CREATE VIEW vbioscale_intelligence_is_sovereign AS
WITH identity_state AS (
    SELECT
        i.bostromaddress,
        i.region,
        i.currentkarma AS k_current,
        s.intelligence_is_sovereign,
        s.protected_stakeholder,
        s.karma_floor,
        s.bi_evidence_mode,
        s.under_attack_state
    FROM identitykarma AS i
    JOIN cyberaug_citizen_sovereign AS s
      ON s.bostromaddress = i.bostromaddress
     AND s.region = i.region
),
bi_invariant AS (
    SELECT
        isv.bostromaddress,
        CASE
            WHEN isv.intelligence_is_sovereign = 1
                 AND isv.bi_evidence_mode NOT IN ('HASHONLY', 'REDACTED')
            THEN 1
            ELSE 0
        END AS bi_mode_violation
    FROM identity_state AS isv
),
karma_invariant AS (
    SELECT
        isv.bostromaddress,
        CASE
            WHEN isv.intelligence_is_sovereign = 1
                 AND isv.protected_stakeholder = 1
                 AND isv.k_current < isv.karma_floor
            THEN 1
            ELSE 0
        END AS karma_floor_violation
    FROM identity_state AS isv
),
underattack_invariant AS (
    SELECT
        isv.bostromaddress,
        CASE
            WHEN isv.intelligence_is_sovereign = 1
                 AND isv.under_attack_state = 'UNDERATTACK'
            THEN 1
            ELSE 0
        END AS under_attack_freeze
    FROM identity_state AS isv
),
aggregate AS (
    SELECT
        isv.bostromaddress,
        isv.region,
        isv.intelligence_is_sovereign,
        isv.protected_stakeholder,
        isv.k_current,
        isv.karma_floor,
        isv.bi_evidence_mode,
        isv.under_attack_state,
        bi.bi_mode_violation,
        kv.karma_floor_violation,
        ua.under_attack_freeze,
        (bi.bi_mode_violation
         + kv.karma_floor_violation
         + ua.under_attack_freeze) AS violation_count
    FROM identity_state AS isv
    JOIN bi_invariant AS bi
      ON bi.bostromaddress = isv.bostromaddress
    JOIN karma_invariant AS kv
      ON kv.bostromaddress = isv.bostromaddress
    JOIN underattack_invariant AS ua
      ON ua.bostromaddress = isv.bostromaddress
)
SELECT
    a.bostromaddress,
    a.region,
    a.intelligence_is_sovereign,
    a.protected_stakeholder,
    a.k_current,
    a.karma_floor,
    a.bi_evidence_mode,
    a.under_attack_state,
    a.bi_mode_violation,
    a.karma_floor_violation,
    a.under_attack_freeze,
    CASE
        WHEN a.intelligence_is_sovereign = 1
             AND a.violation_count > 0
        THEN 1
        ELSE 0
    END AS must_auto_deny
FROM aggregate AS a;
