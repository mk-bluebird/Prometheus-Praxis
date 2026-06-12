-- filename: db/postgres/lake_risk_ker_bundle.sql
-- purpose: Closed-world, static PostgreSQL schema + seed data for lake risk,
--          KER-style scoring, and governance. Designed for AI-chat/KER crawling.
-- notes: No extensions required. Safe to run on a fresh PostgreSQL instance.

------------------------------------------------------------
-- 0. Schema and basic security posture
------------------------------------------------------------

CREATE SCHEMA IF NOT EXISTS eco_lake_risk;

SET search_path TO eco_lake_risk, public;

------------------------------------------------------------
-- 1. Core identity and attribution
------------------------------------------------------------

CREATE TABLE IF NOT EXISTS identity_registry (
    identity_id          SERIAL PRIMARY KEY,
    logical_name         TEXT NOT NULL,
    did_primary          TEXT NOT NULL,
    did_alt              TEXT NOT NULL,
    evm_wallet           TEXT NOT NULL,
    facebook_profile_url TEXT NOT NULL,
    created_at           TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    notes                TEXT NOT NULL DEFAULT ''
);

INSERT INTO identity_registry (
    logical_name,
    did_primary,
    did_alt,
    evm_wallet,
    facebook_profile_url,
    notes
)
VALUES (
    'eco_restoration_shard_primary',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc',
    '0x519fC0eB4111323Cac44b70e1aE31c30e405802D',
    'https://www.facebook.com/profile.php?id=61583146843874',
    'Primary identity for EcoNet / eco_restoration_shard knowledge and lake risk artifacts.'
)
ON CONFLICT DO NOTHING;

------------------------------------------------------------
-- 2. Lake catalog and static ecological context
------------------------------------------------------------

CREATE TABLE IF NOT EXISTS lake (
    lake_id                SERIAL PRIMARY KEY,
    lake_name              TEXT NOT NULL,
    state                  TEXT NOT NULL,
    basin_system           TEXT NOT NULL,
    is_tribal_jurisdiction BOOLEAN NOT NULL DEFAULT FALSE,
    tribal_jurisdiction    TEXT NOT NULL DEFAULT '',
    is_system_critical     BOOLEAN NOT NULL DEFAULT FALSE,
    primary_uses           TEXT NOT NULL, -- e.g., "fisheries, irrigation, hydropower, recreation"
    description            TEXT NOT NULL,
    created_by_identity_id INTEGER NOT NULL REFERENCES identity_registry(identity_id),
    created_at             TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Seed: representative lakes and an aggregate “class” row

INSERT INTO lake (
    lake_name,
    state,
    basin_system,
    is_tribal_jurisdiction,
    tribal_jurisdiction,
    is_system_critical,
    primary_uses,
    description,
    created_by_identity_id
)
SELECT
    'San Carlos Lake',
    'AZ',
    'Gila River / San Carlos Irrigation Project',
    TRUE,
    'San Carlos Apache Nation',
    FALSE,
    'fisheries, recreation, irrigation',
    'Reservoir on the San Carlos Apache Reservation; experienced a catastrophic fish kill and closure in 2026 after prolonged drought, low water levels, and continued dam releases.',
    identity_id
FROM identity_registry
WHERE logical_name = 'eco_restoration_shard_primary'
ON CONFLICT DO NOTHING;

INSERT INTO lake (
    lake_name,
    state,
    basin_system,
    is_tribal_jurisdiction,
    tribal_jurisdiction,
    is_system_critical,
    primary_uses,
    description,
    created_by_identity_id
)
SELECT
    'Lake Powell',
    'AZ/UT',
    'Colorado River – Upper/Lower Basin interface',
    FALSE,
    '',
    TRUE,
    'hydropower, storage, recreation',
    'Second-largest U.S. reservoir by capacity; long-term drought threatens minimum power pool levels and downstream operations.',
    identity_id
FROM identity_registry
WHERE logical_name = 'eco_restoration_shard_primary'
ON CONFLICT DO NOTHING;

INSERT INTO lake (
    lake_name,
    state,
    basin_system,
    is_tribal_jurisdiction,
    tribal_jurisdiction,
    is_system_critical,
    primary_uses,
    description,
    created_by_identity_id
)
SELECT
    'Lake Mead',
    'AZ/NV',
    'Colorado River – Lower Basin',
    FALSE,
    '',
    TRUE,
    'water supply, hydropower, recreation',
    'Major lower Colorado River reservoir supplying multiple states; has experienced record-low elevations and shortage conditions.',
    identity_id
FROM identity_registry
WHERE logical_name = 'eco_restoration_shard_primary'
ON CONFLICT DO NOTHING;

INSERT INTO lake (
    lake_name,
    state,
    basin_system,
    is_tribal_jurisdiction,
    tribal_jurisdiction,
    is_system_critical,
    primary_uses,
    description,
    created_by_identity_id
)
SELECT
    'Roosevelt Lake',
    'AZ',
    'Salt River Project – Salt River Basin',
    FALSE,
    '',
    TRUE,
    'storage, flood control, fisheries, recreation',
    'Large central Arizona reservoir; storage swings and heat can drive algal blooms and oxygen stress in coves.',
    identity_id
FROM identity_registry
WHERE logical_name = 'eco_restoration_shard_primary'
ON CONFLICT DO NOTHING;

INSERT INTO lake (
    lake_name,
    state,
    basin_system,
    is_tribal_jurisdiction,
    tribal_jurisdiction,
    is_system_critical,
    primary_uses,
    description,
    created_by_identity_id
)
SELECT
    'Lake Pleasant',
    'AZ',
    'Central Arizona Project / Agua Fria',
    FALSE,
    '',
    TRUE,
    'water storage, recreation',
    'Phoenix-area storage and recreation reservoir influenced by CAP operations; susceptible to blooms and localized oxygen stress during heat and level swings.',
    identity_id
FROM identity_registry
WHERE logical_name = 'eco_restoration_shard_primary'
ON CONFLICT DO NOTHING;

INSERT INTO lake (
    lake_name,
    state,
    basin_system,
    is_tribal_jurisdiction,
    tribal_jurisdiction,
    is_system_critical,
    primary_uses,
    description,
    created_by_identity_id
)
SELECT
    'Lake Havasu',
    'AZ/CA',
    'Lower Colorado River',
    FALSE,
    '',
    TRUE,
    'water diversion, recreation, fisheries',
    'Key diversion and recreation reservoir on the lower Colorado River; water-quality issues can impact major aqueduct intakes.',
    identity_id
FROM identity_registry
WHERE logical_name = 'eco_restoration_shard_primary'
ON CONFLICT DO NOTHING;

INSERT INTO lake (
    lake_name,
    state,
    basin_system,
    is_tribal_jurisdiction,
    tribal_jurisdiction,
    is_system_critical,
    primary_uses,
    description,
    created_by_identity_id
)
SELECT
    'High-elevation recreational lakes (aggregate class)',
    'AZ',
    'Headwater basins (White Mountains, Mogollon Rim)',
    FALSE,
    '',
    FALSE,
    'cold-water fisheries, recreation, habitat',
    'Aggregate row describing small, cold-water recreational lakes in high-elevation Arizona regions that are sensitive to warming and drought.',
    identity_id
FROM identity_registry
WHERE logical_name = 'eco_restoration_shard_primary'
ON CONFLICT DO NOTHING;

------------------------------------------------------------
-- 3. Official channels and provenance
------------------------------------------------------------

CREATE TABLE IF NOT EXISTS agency_channel (
    channel_id             SERIAL PRIMARY KEY,
    lake_id                INTEGER REFERENCES lake(lake_id),
    basin_system           TEXT NOT NULL,
    agency_name            TEXT NOT NULL,
    channel_type           TEXT NOT NULL, -- "website", "facebook_page", "x_account", "rss"
    url                    TEXT NOT NULL,
    jurisdiction_level     TEXT NOT NULL, -- "federal", "state", "tribal", "county", "local"
    description            TEXT NOT NULL,
    created_by_identity_id INTEGER NOT NULL REFERENCES identity_registry(identity_id),
    created_at             TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Seed: a few core channels (abstracted but structurally correct)

INSERT INTO agency_channel (
    lake_id,
    basin_system,
    agency_name,
    channel_type,
    url,
    jurisdiction_level,
    description,
    created_by_identity_id
)
SELECT
    l.lake_id,
    l.basin_system,
    'San Carlos Recreation and Wildlife Department',
    'facebook_page',
    'https://www.facebook.com/sancarlosrecreationandwildlife',
    'tribal',
    'Primary official channel for San Carlos Lake closures, fish-kill notices, and recreation advisories.',
    i.identity_id
FROM lake l
JOIN identity_registry i ON i.logical_name = 'eco_restoration_shard_primary'
WHERE l.lake_name = 'San Carlos Lake'
ON CONFLICT DO NOTHING;

INSERT INTO agency_channel (
    lake_id,
    basin_system,
    agency_name,
    channel_type,
    url,
    jurisdiction_level,
    description,
    created_by_identity_id
)
SELECT
    NULL,
    'Arizona statewide',
    'Arizona Game and Fish Department',
    'website',
    'https://www.azgfd.com',
    'state',
    'State agency for wildlife, fisheries, HAB advisories, and fish-kill reports across Arizona.',
    i.identity_id
FROM identity_registry i
WHERE i.logical_name = 'eco_restoration_shard_primary'
ON CONFLICT DO NOTHING;

INSERT INTO agency_channel (
    lake_id,
    basin_system,
    agency_name,
    channel_type,
    url,
    jurisdiction_level,
    description,
    created_by_identity_id
)
SELECT
    (SELECT lake_id FROM lake WHERE lake_name = 'Lake Powell'),
    'Colorado River – Upper/Lower Basin interface',
    'U.S. Bureau of Reclamation',
    'website',
    'https://www.usbr.gov',
    'federal',
    'Federal agency providing elevation and release information for Lake Powell and other Colorado River reservoirs.',
    i.identity_id
FROM identity_registry i
WHERE i.logical_name = 'eco_restoration_shard_primary'
ON CONFLICT DO NOTHING;

------------------------------------------------------------
-- 4. Lake risk, early-warning signals, and notes
------------------------------------------------------------

CREATE TABLE IF NOT EXISTS lake_risk (
    lake_risk_id          SERIAL PRIMARY KEY,
    lake_id               INTEGER NOT NULL REFERENCES lake(lake_id),
    primary_risk_type     TEXT NOT NULL,
    early_warning_signals TEXT NOT NULL,
    management_agencies   TEXT NOT NULL,
    key_agency_contacts   TEXT NOT NULL,
    notes                 TEXT NOT NULL,
    source_refs           TEXT NOT NULL, -- textual citations/URLs, not live links
    created_by_identity_id INTEGER NOT NULL REFERENCES identity_registry(identity_id),
    created_at            TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Seed: San Carlos as the archetypal failure mode

INSERT INTO lake_risk (
    lake_id,
    primary_risk_type,
    early_warning_signals,
    management_agencies,
    key_agency_contacts,
    notes,
    source_refs,
    created_by_identity_id
)
SELECT
    l.lake_id,
    'Ecological collapse and public-health risk from drought-driven fish kill',
    'Sustained reservoir levels near or below a few percent of capacity; prolonged high air and surface-water temperatures; algal blooms and turbid, odorous water; declining dissolved oxygen approaching critical thresholds; ongoing mandatory dam releases despite critically low storage; community and agency observations of stressed or dying fish.',
    'San Carlos Recreation and Wildlife Department; San Carlos Apache Tribal leadership; Arizona Game and Fish Department; federal partners as needed.',
    'San Carlos Recreation and Wildlife Department public notices; San Carlos Apache Nation administration; Arizona Game and Fish Department main line; regional offices for fisheries and habitat.',
    'San Carlos Lake experienced a catastrophic fish kill and closure in 2026. Years of drought and low storage, combined with heat and continued releases, led to an oxygen crash that killed nearly all fish and created a public-health hazard, forcing closure of fishing and shoreline recreation.',
    'Tribal and local notices describing the 2026 San Carlos closure and fish kill; state and media coverage documenting drought, low water, and continued releases as contributing factors.',
    i.identity_id
FROM lake l
JOIN identity_registry i ON i.logical_name = 'eco_restoration_shard_primary'
WHERE l.lake_name = 'San Carlos Lake'
ON CONFLICT DO NOTHING;

-- Seed: Lake Powell, Lake Mead, and others as risk patterns

INSERT INTO lake_risk (
    lake_id,
    primary_risk_type,
    early_warning_signals,
    management_agencies,
    key_agency_contacts,
    notes,
    source_refs,
    created_by_identity_id
)
SELECT
    l.lake_id,
    'Structural low-reservoir risk with associated ecological stress',
    'Projected or observed elevations approaching minimum power pool; long-term drought; warm surface layers and increased residence time; reports of algal blooms and degraded near-shore water quality.',
    'U.S. Bureau of Reclamation; National Park Service; affected state and tribal water agencies.',
    'Bureau of Reclamation operations and communications offices; Glen Canyon National Recreation Area resource staff; state wildlife agencies.',
    'Lake Powell operates under Colorado River rules with drought and over-allocation pressures. Low elevations threaten hydropower and can worsen water-quality and habitat conditions in embayments.',
    'Federal operating condition reports, drought analyses, and recreation resource reports describing low-elevation impacts at Lake Powell.',
    i.identity_id
FROM lake l
JOIN identity_registry i ON i.logical_name = 'eco_restoration_shard_primary'
WHERE l.lake_name = 'Lake Powell'
ON CONFLICT DO NOTHING;

------------------------------------------------------------
-- 5. Social signals (early-warning mesh)
------------------------------------------------------------

CREATE TABLE IF NOT EXISTS social_signal (
    signal_id             SERIAL PRIMARY KEY,
    lake_id               INTEGER REFERENCES lake(lake_id),
    lake_name_text        TEXT NOT NULL,
    platform              TEXT NOT NULL, -- e.g., "facebook", "x", "news_site"
    url                   TEXT NOT NULL,
    event_type            TEXT NOT NULL, -- "fish_kill", "hab", "closure", "low_water", etc.
    severity              TEXT NOT NULL, -- "low", "medium", "high"
    status                TEXT NOT NULL, -- "unverified", "seeking_confirmation", "confirmed_by_official", "dismissed"
    received_at           TIMESTAMPTZ NOT NULL,
    summary               TEXT NOT NULL,
    created_by_identity_id INTEGER NOT NULL REFERENCES identity_registry(identity_id),
    created_at            TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Example generic pattern rows (no live URLs, but structurally useful)

INSERT INTO social_signal (
    lake_id,
    lake_name_text,
    platform,
    url,
    event_type,
    severity,
    status,
    received_at,
    summary,
    created_by_identity_id
)
SELECT
    (SELECT lake_id FROM lake WHERE lake_name = 'San Carlos Lake'),
    'San Carlos Lake',
    'facebook',
    'https://example.org/san_carlos_lake_fish_kill_post',
    'fish_kill',
    'high',
    'confirmed_by_official',
    NOW() - INTERVAL '0 days',
    'Public post reporting thousands of dead fish along the shoreline and referencing an official closure notice.',
    i.identity_id
FROM identity_registry i
WHERE i.logical_name = 'eco_restoration_shard_primary'
ON CONFLICT DO NOTHING;

------------------------------------------------------------
-- 6. KER-style risk scoring and metrics
------------------------------------------------------------

CREATE TABLE IF NOT EXISTS lake_risk_score (
    score_id              SERIAL PRIMARY KEY,
    lake_id               INTEGER NOT NULL REFERENCES lake(lake_id),
    assessment_label      TEXT NOT NULL,  -- e.g., "initial_static", "post_event_review"
    timestamp             TIMESTAMPTZ NOT NULL,
    risk_score_numeric    REAL NOT NULL,  -- 0..100
    knowledge_factor      REAL NOT NULL,  -- 0..100
    eco_impact_value      REAL NOT NULL,  -- 0..100
    harm_risk_flag        BOOLEAN NOT NULL,
    rationale             TEXT NOT NULL,
    created_by_identity_id INTEGER NOT NULL REFERENCES identity_registry(identity_id),
    created_at            TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Seed: static example scores for AI/KER to learn from

INSERT INTO lake_risk_score (
    lake_id,
    assessment_label,
    timestamp,
    risk_score_numeric,
    knowledge_factor,
    eco_impact_value,
    harm_risk_flag,
    rationale,
    created_by_identity_id
)
SELECT
    l.lake_id,
    'initial_static',
    NOW(),
    95.0,
    90.0,
    75.0,
    TRUE,
    'San Carlos Lake post-2026 fish kill: extremely high risk of ecological collapse recurrence if operations and storage thresholds are not changed. Strong evidentiary base from tribal, state, and media sources.',
    i.identity_id
FROM lake l
JOIN identity_registry i ON i.logical_name = 'eco_restoration_shard_primary'
WHERE l.lake_name = 'San Carlos Lake'
ON CONFLICT DO NOTHING;

INSERT INTO lake_risk_score (
    lake_id,
    assessment_label,
    timestamp,
    risk_score_numeric,
    knowledge_factor,
    eco_impact_value,
    harm_risk_flag,
    rationale,
    created_by_identity_id
)
SELECT
    l.lake_id,
    'initial_static',
    NOW(),
    80.0,
    85.0,
    95.0,
    TRUE,
    'Lake Powell: high structural risk due to long-term drought and allocation pressure; very high eco-impact given its role in Colorado River operations.',
    i.identity_id
FROM lake l
JOIN identity_registry i ON i.logical_name = 'eco_restoration_shard_primary'
WHERE l.lake_name = 'Lake Powell'
ON CONFLICT DO NOTHING;

------------------------------------------------------------
-- 7. Rule templates and governance (for AI and humans)
------------------------------------------------------------

CREATE TABLE IF NOT EXISTS rule_template (
    rule_id               SERIAL PRIMARY KEY,
    rule_name             TEXT NOT NULL,
    rule_description      TEXT NOT NULL,
    condition_expression  TEXT NOT NULL,
    action_description    TEXT NOT NULL,
    created_by_identity_id INTEGER NOT NULL REFERENCES identity_registry(identity_id),
    created_at            TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

INSERT INTO rule_template (
    rule_name,
    rule_description,
    condition_expression,
    action_description,
    created_by_identity_id
)
SELECT
    'san_carlos_style_failure_mode',
    'Detects conditions similar to the San Carlos Lake 2026 collapse: extremely low storage, heat, blooms, and continued releases.',
    'IF storage_percent < 5 AND sustained_high_temperature = TRUE AND bloom_signals = TRUE AND continued_releases = TRUE THEN escalate_to_emergency_review',
    'Escalate lake to emergency review by human stewards; do not auto-issue public alerts without human authorization.',
    i.identity_id
FROM identity_registry i
WHERE i.logical_name = 'eco_restoration_shard_primary'
ON CONFLICT DO NOTHING;

CREATE TABLE IF NOT EXISTS governance_event (
    governance_event_id   SERIAL PRIMARY KEY,
    lake_id               INTEGER REFERENCES lake(lake_id),
    event_type            TEXT NOT NULL, -- "alert_approved", "alert_rejected", "tribal_consultation", etc.
    description           TEXT NOT NULL,
    decided_by            TEXT NOT NULL, -- human description, not PII
    timestamp             TIMESTAMPTZ NOT NULL,
    created_by_identity_id INTEGER NOT NULL REFERENCES identity_registry(identity_id),
    created_at            TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

------------------------------------------------------------
-- 8. Views to simplify AI/KER access
------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_lake_risk_latest AS
SELECT
    l.lake_name,
    l.state,
    l.basin_system,
    l.is_tribal_jurisdiction,
    l.is_system_critical,
    lr.primary_risk_type,
    lr.early_warning_signals,
    lr.notes,
    lrs.risk_score_numeric,
    lrs.knowledge_factor,
    lrs.eco_impact_value,
    lrs.harm_risk_flag
FROM lake l
LEFT JOIN lake_risk lr
    ON lr.lake_id = l.lake_id
LEFT JOIN LATERAL (
    SELECT *
    FROM lake_risk_score s
    WHERE s.lake_id = l.lake_id
    ORDER BY s.timestamp DESC
    LIMIT 1
) lrs ON TRUE;

------------------------------------------------------------
-- 9. End of bundle
------------------------------------------------------------
