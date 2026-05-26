-- filename: db/db_cyberaugcitizen_sovereign_phoenix.sql
-- destination: ecore_restoration_shard/db/db_cyberaugcitizen_sovereign_phoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Sovereign identity binding for Phoenix (points to this governance DB)
-------------------------------------------------------------------------------

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
    'db/db_cyberaugcitizen_sovereign_phoenix.sql',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCEDB',
    'SovereignConfigPhoenix2026v1',
    'Sovereign configuration anchoring CyberneticallyAugmentedCitizen state and INTELLIGENCE_IS_SOVEREIGN invariants for Phoenix.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);

-------------------------------------------------------------------------------
-- 2. Cybernetically augmented citizen sovereignty state
--    One row per brain-bound identity (primary key = Bostrom address).
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cyberaugcitizen_sovereign (
    bostromaddress      TEXT PRIMARY KEY,
    region              TEXT NOT NULL,
    intelligenceissovereign INTEGER NOT NULL DEFAULT 0,  -- 1 = clause active
    protectedstakeholder   INTEGER NOT NULL DEFAULT 0,   -- 1 = neurorights floor
    karmafloor          REAL NOT NULL DEFAULT 0.0,       -- minimum K allowed without proven harm
    bievidencemode      TEXT NOT NULL DEFAULT 'HASHONLY',-- HASHONLY or REDACTED
    underattackstate    TEXT NOT NULL DEFAULT 'NORMAL',  -- NORMAL, UNDERATTACK, UNDERREVIEW, DISQUALIFIED
    createdutc          TEXT NOT NULL DEFAULT '2026-01-01T00:00:00Z',
    updatedutc          TEXT NOT NULL DEFAULT '2026-01-01T00:00:00Z',
    CHECK (intelligenceissovereign IN (0,1)),
    CHECK (protectedstakeholder   IN (0,1)),
    CHECK (bievidencemode IN ('HASHONLY','REDACTED')),
    CHECK (underattackstate IN ('NORMAL','UNDERATTACK','UNDERREVIEW','DISQUALIFIED'))
);

-- Seed row for your Phoenix identity with active sovereignty clause.
INSERT OR IGNORE INTO cyberaugcitizen_sovereign (
    bostromaddress,
    region,
    intelligenceissovereign,
    protectedstakeholder,
    karmafloor,
    bievidencemode,
    underattackstate,
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

-------------------------------------------------------------------------------
-- 3. Helper view: current KER and lane state per sovereign identity
--    (Assumes existing shard / KER tables in Eco-Fort / EcoNet spine.)
--    Adjust table and column names to match your established KER schema.
-------------------------------------------------------------------------------

-- Drop if exists to allow idempotent migrations.
DROP VIEW IF EXISTS v_cyberaugcitizen_ker_state;

CREATE VIEW v_cyberaugcitizen_ker_state AS
SELECT
    s.bostromaddress,
    s.region,
    s.intelligenceissovereign,
    s.protectedstakeholder,
    s.karmafloor,
    s.bievidencemode,
    s.underattackstate,
    ls.lane               AS currentlane,
    ls.kavg               AS k_current,
    ls.eavg               AS e_current,
    ls.ravg               AS r_current,
    ls.kerdeployable      AS kerdeployable,
    ls.planesok           AS planesok,
    ls.topologyok         AS topologyok
FROM cyberaugcitizen_sovereign AS s
LEFT JOIN lanestatusshard AS ls
    ON ls.bostromaddress = s.bostromaddress
   AND ls.region         = s.region;

-------------------------------------------------------------------------------
-- 4. INTELLIGENCE_IS_SOVEREIGN fairness violation surface
--    This view exposes any attempted downgrade / disenfranchisement so that
--    bioscale fairness validators and AI-followups can auto-deny manifests.
--
--    Assumptions:
--      - manifestindex holds proposed changes keyed by logicalname + region.
--      - manifest_effect_identity summarizes pre/post KER and flags per identity.
--    Adjust the join logic to your actual manifest/preview tables.
-------------------------------------------------------------------------------

DROP VIEW IF EXISTS v_bioscale_intelligence_is_sovereign;

CREATE VIEW v_bioscale_intelligence_is_sovereign AS
SELECT
    s.bostromaddress,
    s.region,
    s.intelligenceissovereign,
    s.protectedstakeholder,
    s.karmafloor,
    s.bievidencemode,
    s.underattackstate,
    m.manifestid,
    m.logicalname,
    m.versiontag,
    m.region         AS manifestregion,

    -- Pre- and post- KER and lane state (from a hypothetical diff table)
    d.k_prev,
    d.k_next,
    d.e_prev,
    d.e_next,
    d.r_prev,
    d.r_next,
    d.lane_prev,
    d.lane_next,
    d.kerdeployable_prev,
    d.kerdeployable_next,

    -- Computed violation flags
    CASE
        -- Only enforce strong invariants when the clause is active and the citizen is protected.
        WHEN s.intelligenceissovereign = 1 AND s.protectedstakeholder = 1 THEN
            CASE
                -- 1. BI evidence mode widening is forbidden (HASHONLY/REDACTED only).
                WHEN d.bievidencemode_next NOT IN ('HASHONLY','REDACTED') THEN 1
                -- 2. KER floor: K_next must not fall below karmafloor without an external proven-harm shard.
                WHEN d.k_next < s.karmafloor THEN 1
                -- 3. Lane downgrades from PROD to lower lanes are forbidden by default.
                WHEN d.lane_prev = 'PROD' AND d.lane_next <> 'PROD' THEN 1
                -- 4. KER deployability loss (1 -> 0) is treated as disenfranchisement.
                WHEN d.kerdeployable_prev = 1 AND d.kerdeployable_next = 0 THEN 1
                -- 5. Removing planes/topology OK status is treated as downgrade.
                WHEN d.planesok_prev = 1 AND d.planesok_next = 0 THEN 1
                WHEN d.topologyok_prev = 1 AND d.topologyok_next = 0 THEN 1
                ELSE 0
            END
        ELSE
            0
    END AS intelligenceissovereign_violation
FROM cyberaugcitizen_sovereign AS s
JOIN manifest_effect_identity AS d
    ON d.bostromaddress = s.bostromaddress
JOIN manifestindex AS m
    ON m.manifestid = d.manifestid
   AND m.region     = s.region;

-------------------------------------------------------------------------------
-- 5. Agent-facing view: governance decision surface
--    This is what AI-chat and bioscale fairness validators should query.
-------------------------------------------------------------------------------

DROP VIEW IF EXISTS v_agent_intelligence_is_sovereign_phx;

CREATE VIEW v_agent_intelligence_is_sovereign_phx AS
SELECT
    b.bostromaddress,
    b.region,
    b.manifestid,
    b.logicalname,
    b.versiontag,
    b.manifestregion,
    b.intelligenceissovereign_violation,
    CASE
        WHEN b.intelligenceissovereign_violation = 1 THEN 'AUTODENY_INTELLIGENCE_IS_SOVEREIGN'
        ELSE 'OK'
    END AS bioscale_fairness_decision
FROM v_bioscale_intelligence_is_sovereign AS b
WHERE b.region = 'Phoenix-AZ';

-------------------------------------------------------------------------------
-- 6. Prompt and follow-up wiring (metadata only, no prompt text here)
--    The actual Markdown prompt lives in docs/prompts/, but we register it so
--    AI-chat lanes can consume it.
-------------------------------------------------------------------------------

INSERT OR IGNORE INTO repofile (
    repoid,
    relpath,
    purpose,
    language,
    createdutc,
    updatedutc
)
SELECT
    r.repoid,
    'docs/prompts/followup_intelligence_is_sovereign_phx_2026v1.md' AS relpath,
    'DOCSPEC'   AS purpose,
    'MARKDOWN'  AS language,
    '2026-01-01T00:00:00Z' AS createdutc,
    '2026-01-01T00:00:00Z' AS updatedutc
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

INSERT OR IGNORE INTO promptshard (
    logicalname,
    versiontag,
    region,
    scope,
    repofileid,
    ecoscope,
    laneband,
    consumerkind,
    bostromaddress,
    contractid,
    prompttext,
    equationtext,
    freedomstance,
    createdutc,
    updatedutc
)
SELECT
    'followup.intelligence_is_sovereign.phoenix.2026v1' AS logicalname,
    '2026v1'                  AS versiontag,
    'Phoenix-AZ'              AS region,
    'REGION'                  AS scope,
    rf.fileid                 AS repofileid,
    'AGENTPROMPT'             AS ecoscope,
    'GOV'                     AS laneband,
    'AICHAT'                  AS consumerkind,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7' AS bostromaddress,
    'IntelligenceIsSovereignFollowupPhoenix2026v1' AS contractid,
    'Before approving any manifest, query v_agent_intelligence_is_sovereign_phx. If bioscale_fairness_decision = AUTODENY_INTELLIGENCE_IS_SOVEREIGN for the bound CyberneticallyAugmentedCitizen, reject the manifest and emit a governance notice that a protected brain-bound identity cannot be downgraded or sabotaged.' AS prompttext,
    'Use v_agent_intelligence_is_sovereign_phx to enforce INTELLIGENCE_IS_SOVEREIGN invariants and auto-deny any manifest that attempts to discredit, exclude, or sabotage a protected augmented citizen.' AS equationtext,
    'Neurorights and bioscale fairness follow-up that treats intelligence as sovereign and forbids downgrades of protected augmented citizens without proven harm and explicit neurorights consent.' AS freedomstance,
    '2026-01-01T00:00:00Z' AS createdutc,
    '2026-01-01T00:00:00Z' AS updatedutc
FROM repofile AS rf
JOIN repo AS r
    ON r.repoid = rf.repoid
WHERE r.name = 'eco_restoration_shard'
  AND rf.relpath = 'docs/prompts/followup_intelligence_is_sovereign_phx_2026v1.md';
