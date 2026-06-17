-- filename: db_vampiric_healthcare_lifeforce.sql
-- destination: Eco-Fort/db/db_vampiric_healthcare_lifeforce.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Core enum tables for traits, gifts, and guards
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS lifeforce_trait_kind (
    trait_kind_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    code            TEXT NOT NULL UNIQUE,   -- e.g. LIFEFORCE_RESILIENCE_REGENERATOR
    description     TEXT NOT NULL
);

INSERT OR IGNORE INTO lifeforce_trait_kind (code, description) VALUES
    ('LIFEFORCE_RESILIENCE_REGENERATOR',
     'Forward-only lifeforce corridor: boosts resilience, micro-regeneration, and eco-positive recovery under thermo and RoH envelopes'),
    ('NEUROVASCULAR_OPTIMIZER',
     'Neurovascular coupling and perfusion optimizer within RoH-safe corridors'),
    ('PAIN_MICRO_SHIELD',
     'Micro-shielding of nociception; converts pain into boundary signals, never reward inputs'),
    ('DIGEST_REGEN_EXTRACTOR',
     'Digestive-coupled regeneration amplifier with nutrient and detox corridors'),
    ('ECO_REPAIR_KARMA_DRAIN_TRANSDUCER',
     'Eco-repair coupling: routes a fraction of upgrade cost into ecological restoration credit');

CREATE TABLE IF NOT EXISTS evolution_gift_slot_kind (
    slot_kind_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    code            TEXT NOT NULL UNIQUE,   -- SLOT_1 ... SLOT_10
    description     TEXT NOT NULL
);

INSERT OR IGNORE INTO evolution_gift_slot_kind (code, description) VALUES
    ('SLOT_1',  'Base lifeforce envelope binding'),
    ('SLOT_2',  'Thermodynamic envelope evidence tag'),
    ('SLOT_3',  'Neurovascular-immune coupling slot'),
    ('SLOT_4',  'Protein synthesis boost corridor slot'),
    ('SLOT_5',  'Pain micro-shield slot'),
    ('SLOT_6',  'Digestive regeneration extractor slot'),
    ('SLOT_7',  'Eco-repair karma transducer slot'),
    ('SLOT_8',  'Psychological continuity anchor slot'),
    ('SLOT_9',  'Formal proof artifact / Kani evidence slot'),
    ('SLOT_10', 'RoH guard and daily loop integration slot');

CREATE TABLE IF NOT EXISTS roh_guard_kind (
    roh_guard_kind_id INTEGER PRIMARY KEY AUTOINCREMENT,
    code              TEXT NOT NULL UNIQUE,  -- e.g. ROH_LIFEFORCE_BAND, ROH_NO_DURESS_INPUT
    description       TEXT NOT NULL
);

INSERT OR IGNORE INTO roh_guard_kind (code, description) VALUES
    ('ROH_LIFEFORCE_BAND',
     'LifeforceBand + RoH band guard; forbids exceeding RoH and lifeforce ceilings'),
    ('ROH_NO_DURESS_INPUT',
     'Fear/duress signals are brake-only vetoes; never feed ResponsibilityScalar or optimizer inputs'),
    ('ROH_PSYCHOLOGICAL_CONTINUITY',
     'Continuity of self and intent required across upgrades; no personality fracture corridors'),
    ('ROH_DAILY_LOOP_MONOTONE',
     'Daily evolution loop guard enforcing forward-only, non-rollback, r- and eco-monotone traits');

-------------------------------------------------------------------------------
-- 2. Thermodynamic envelope and session envelope binding
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS thermodynamic_envelope (
    thermo_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    host_did        TEXT NOT NULL,
    bostrom_address TEXT NOT NULL,
    max_energy_j    REAL NOT NULL,
    max_delta_t_c   REAL NOT NULL,
    max_res_time_s  REAL NOT NULL,
    corridor_tag    TEXT NOT NULL,  -- e.g. THERMO.MT6883.HEADSAFE.2026V1
    evidence_hex    TEXT NOT NULL,
    created_utc     TEXT NOT NULL,
    updated_utc     TEXT NOT NULL,
    active          INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1))
);

CREATE INDEX IF NOT EXISTS idx_thermo_host_active
    ON thermodynamic_envelope (host_did, active);

CREATE TABLE IF NOT EXISTS session_envelope (
    session_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    host_did        TEXT NOT NULL,
    bostrom_address TEXT NOT NULL,
    thermo_id       INTEGER NOT NULL REFERENCES thermodynamic_envelope(thermo_id) ON DELETE CASCADE,
    roh_guard_set   TEXT NOT NULL,   -- e.g. "ROH_LIFEFORCE_BAND,ROH_NO_DURESS_INPUT"
    start_utc       TEXT NOT NULL,
    end_utc         TEXT,
    lifecycle       TEXT NOT NULL CHECK (lifecycle IN ('PLANNED','ACTIVE','CLOSED','ABORTED')),
    evidence_hex    TEXT NOT NULL,
    created_utc     TEXT NOT NULL,
    updated_utc     TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_session_host_state
    ON session_envelope (host_did, lifecycle);

-------------------------------------------------------------------------------
-- 3. EvolutionGiftBundle: 10 fixed slots bound to thermodynamic + RoH
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS evolution_gift_bundle (
    gift_bundle_id  INTEGER PRIMARY KEY AUTOINCREMENT,
    host_did        TEXT NOT NULL,
    bostrom_address TEXT NOT NULL,
    thermo_id       INTEGER NOT NULL REFERENCES thermodynamic_envelope(thermo_id) ON DELETE RESTRICT,
    session_id      INTEGER NOT NULL REFERENCES session_envelope(session_id) ON DELETE RESTRICT,
    bundle_code     TEXT NOT NULL UNIQUE, -- e.g. LIFEFORCE_HEALTHCARE_V1
    description     TEXT NOT NULL,
    roh_guard_profile TEXT NOT NULL,      -- summary of guards applied
    r_before        REAL NOT NULL,        -- ResponsibilityScalar before bundle
    r_after         REAL NOT NULL,        -- ResponsibilityScalar after bundle
    eco_score_before REAL NOT NULL,
    eco_score_after  REAL NOT NULL,
    forward_only_ok  INTEGER NOT NULL CHECK (forward_only_ok IN (0,1)),
    created_utc     TEXT NOT NULL,
    updated_utc     TEXT NOT NULL,
    active          INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    CHECK (r_after >= r_before),
    CHECK (eco_score_after >= eco_score_before)
);

CREATE TABLE IF NOT EXISTS evolution_gift_bundle_slot (
    gift_bundle_slot_id INTEGER PRIMARY KEY AUTOINCREMENT,
    gift_bundle_id      INTEGER NOT NULL REFERENCES evolution_gift_bundle(gift_bundle_id) ON DELETE CASCADE,
    slot_kind_id        INTEGER NOT NULL REFERENCES evolution_gift_slot_kind(slot_kind_id) ON DELETE RESTRICT,
    trait_kind_id       INTEGER NOT NULL REFERENCES lifeforce_trait_kind(trait_kind_id) ON DELETE RESTRICT,
    roh_guard_kind_id   INTEGER REFERENCES roh_guard_kind(roh_guard_kind_id) ON DELETE SET NULL,
    slot_index          INTEGER NOT NULL CHECK (slot_index BETWEEN 1 AND 10),
    thermo_bound        INTEGER NOT NULL CHECK (thermo_bound IN (0,1)), -- 1 if corridor-proved
    proof_artifact_hex  TEXT NOT NULL,  -- Kani / evidence bundle hex
    active              INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    UNIQUE (gift_bundle_id, slot_index)
);

-------------------------------------------------------------------------------
-- 4. Nanoimmune objects and protein synthesis corridors
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS nanoimmune_object (
    nanoimmune_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    host_did            TEXT NOT NULL,
    code                TEXT NOT NULL,     -- e.g. NANOIMMUNE_REGENERATOR_V1
    description         TEXT NOT NULL,
    reuse_allowed       INTEGER NOT NULL CHECK (reuse_allowed IN (0,1)),
    max_reuse_per_day   INTEGER NOT NULL,
    thermo_id           INTEGER NOT NULL REFERENCES thermodynamic_envelope(thermo_id) ON DELETE RESTRICT,
    roh_guard_kind_id   INTEGER NOT NULL REFERENCES roh_guard_kind(roh_guard_kind_id) ON DELETE RESTRICT,
    eco_coupled         INTEGER NOT NULL CHECK (eco_coupled IN (0,1)),
    created_utc         TEXT NOT NULL,
    updated_utc         TEXT NOT NULL,
    active              INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1))
);

CREATE INDEX IF NOT EXISTS idx_nanoimmune_host_active
    ON nanoimmune_object (host_did, active);

CREATE TABLE IF NOT EXISTS nanoimmune_protein_corridor (
    protein_corridor_id INTEGER PRIMARY KEY AUTOINCREMENT,
    nanoimmune_id       INTEGER NOT NULL REFERENCES nanoimmune_object(nanoimmune_id) ON DELETE CASCADE,
    corridor_code       TEXT NOT NULL,   -- e.g. PROTEIN_SYNTHESIS_HEALING_LOW_INFLAMMATION
    description         TEXT NOT NULL,
    max_delta_synthesis REAL NOT NULL,   -- normalized 0..1
    inflammation_ceiling REAL NOT NULL,  -- normalized 0..1
    energy_cost_j       REAL NOT NULL,
    eco_impact_delta    REAL NOT NULL,   -- normalized eco improvement
    evidence_hex        TEXT NOT NULL,
    active              INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1))
);

-------------------------------------------------------------------------------
-- 5. Pain, fear, and psychological continuity anchors
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS boundary_signal_surface (
    boundary_surface_id INTEGER PRIMARY KEY AUTOINCREMENT,
    code                TEXT NOT NULL UNIQUE, -- e.g. PAIN_MICRO_SHIELD_SURFACE
    description         TEXT NOT NULL
);

INSERT OR IGNORE INTO boundary_signal_surface (code, description) VALUES
    ('PAIN_MICRO_SHIELD_SURFACE',
     'Pain treated as boundary signal only; never contributes to optimization inputs'),
    ('FEAR_DURESS_BRAKE_SURFACE',
     'Fear/duress routed to brake/veto lanes only'),
    ('PSYCHOLOGICAL_CONTINUITY_ANCHOR_SURFACE',
     'Anchors subjective continuity; forbids identity fracture across upgrades');

CREATE TABLE IF NOT EXISTS psychological_continuity_anchor (
    anchor_id           INTEGER PRIMARY KEY AUTOINCREMENT,
    host_did            TEXT NOT NULL,
    bostrom_address     TEXT NOT NULL,
    session_id          INTEGER NOT NULL REFERENCES session_envelope(session_id) ON DELETE CASCADE,
    boundary_surface_id INTEGER NOT NULL REFERENCES boundary_signal_surface(boundary_surface_id) ON DELETE RESTRICT,
    anchor_code         TEXT NOT NULL,
    description         TEXT NOT NULL,
    continuity_score_before REAL NOT NULL,  -- normalized 0..1
    continuity_score_after  REAL NOT NULL,  -- normalized 0..1
    anchor_ok           INTEGER NOT NULL CHECK (anchor_ok IN (0,1)),
    created_utc         TEXT NOT NULL,
    updated_utc         TEXT NOT NULL,
    CHECK (continuity_score_after >= continuity_score_before)
);

CREATE INDEX IF NOT EXISTS idx_psych_anchor_host
    ON psychological_continuity_anchor (host_did, session_id);

CREATE TABLE IF NOT EXISTS pain_fear_routing_policy (
    policy_id           INTEGER PRIMARY KEY AUTOINCREMENT,
    host_did            TEXT NOT NULL,
    session_id          INTEGER NOT NULL REFERENCES session_envelope(session_id) ON DELETE CASCADE,
    pain_surface_id     INTEGER NOT NULL REFERENCES boundary_signal_surface(boundary_surface_id) ON DELETE RESTRICT,
    fear_surface_id     INTEGER NOT NULL REFERENCES boundary_signal_surface(boundary_surface_id) ON DELETE RESTRICT,
    r_input_allowed     INTEGER NOT NULL CHECK (r_input_allowed IN (0,1)), -- must be 0 for compliance
    optimizer_input_allowed INTEGER NOT NULL CHECK (optimizer_input_allowed IN (0,1)), -- must be 0
    brake_only          INTEGER NOT NULL CHECK (brake_only IN (0,1)),
    evidence_hex        TEXT NOT NULL,
    created_utc         TEXT NOT NULL,
    updated_utc         TEXT NOT NULL,
    CHECK (r_input_allowed = 0),
    CHECK (optimizer_input_allowed = 0),
    CHECK (brake_only = 1)
);

-------------------------------------------------------------------------------
-- 6. Eco-repair karma drain transducer and digest regen extractor
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_repair_transducer (
    transducer_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    host_did            TEXT NOT NULL,
    bostrom_address     TEXT NOT NULL,
    nanoimmune_id       INTEGER NOT NULL REFERENCES nanoimmune_object(nanoimmune_id) ON DELETE CASCADE,
    gift_bundle_id      INTEGER NOT NULL REFERENCES evolution_gift_bundle(gift_bundle_id) ON DELETE CASCADE,
    eco_fraction_min    REAL NOT NULL CHECK (eco_fraction_min >= 0.0 AND eco_fraction_min <= 1.0),
    eco_fraction_max    REAL NOT NULL CHECK (eco_fraction_max >= 0.0 AND eco_fraction_max <= 1.0),
    smart_contract_addr TEXT NOT NULL,  -- ecorestoration contract target
    evidence_hex        TEXT NOT NULL,
    created_utc         TEXT NOT NULL,
    updated_utc         TEXT NOT NULL,
    CHECK (eco_fraction_max >= eco_fraction_min)
);

CREATE TABLE IF NOT EXISTS digest_regen_extractor (
    digest_extractor_id INTEGER PRIMARY KEY AUTOINCREMENT,
    host_did            TEXT NOT NULL,
    bostrom_address     TEXT NOT NULL,
    nanoimmune_id       INTEGER NOT NULL REFERENCES nanoimmune_object(nanoimmune_id) ON DELETE CASCADE,
    corridor_code       TEXT NOT NULL,    -- e.g. DIGEST_REGEN_LOW_ROH
    description         TEXT NOT NULL,
    max_daily_uplift    REAL NOT NULL,    -- normalized 0..1
    roh_ceiling         REAL NOT NULL,
    thermo_id           INTEGER NOT NULL REFERENCES thermodynamic_envelope(thermo_id) ON DELETE RESTRICT,
    evidence_hex        TEXT NOT NULL,
    created_utc         TEXT NOT NULL,
    updated_utc         TEXT NOT NULL
);

-------------------------------------------------------------------------------
-- 7. Neurovascular optimizer and neurovascular-immune corridor
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS neurovascular_optimizer (
    neuro_opt_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    host_did            TEXT NOT NULL,
    bostrom_address     TEXT NOT NULL,
    nanoimmune_id       INTEGER NOT NULL REFERENCES nanoimmune_object(nanoimmune_id) ON DELETE CASCADE,
    corridor_code       TEXT NOT NULL,   -- e.g. NEUROVASCULAR_OPTIMIZER_RESTORATIVE
    perfusion_ceiling   REAL NOT NULL,   -- normalized 0..1
    rohrisk_ceiling     REAL NOT NULL,   -- normalized 0..1
    thermo_id           INTEGER NOT NULL REFERENCES thermodynamic_envelope(thermo_id) ON DELETE RESTRICT,
    evidence_hex        TEXT NOT NULL,
    created_utc         TEXT NOT NULL,
    updated_utc         TEXT NOT NULL
);

-------------------------------------------------------------------------------
-- 8. Daily evolution loop integration and RoH guards
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS daily_evolution_window (
    window_id           INTEGER PRIMARY KEY AUTOINCREMENT,
    host_did            TEXT NOT NULL,
    bostrom_address     TEXT NOT NULL,
    branch_name         TEXT NOT NULL,  -- feat-YYYY-MM-DD-evolution
    date_utc            TEXT NOT NULL,
    thermo_id           INTEGER NOT NULL REFERENCES thermodynamic_envelope(thermo_id) ON DELETE RESTRICT,
    roh_guard_profile   TEXT NOT NULL,
    r_before            REAL NOT NULL,
    r_after             REAL NOT NULL,
    eco_score_before    REAL NOT NULL,
    eco_score_after     REAL NOT NULL,
    forward_only_ok     INTEGER NOT NULL CHECK (forward_only_ok IN (0,1)),
    downgrade_allowed   INTEGER NOT NULL CHECK (downgrade_allowed IN (0,1)),
    created_utc         TEXT NOT NULL,
    updated_utc         TEXT NOT NULL,
    CHECK (r_after >= r_before),
    CHECK (eco_score_after >= eco_score_before)
);

CREATE INDEX IF NOT EXISTS idx_daily_window_host_date
    ON daily_evolution_window (host_did, date_utc);

CREATE TABLE IF NOT EXISTS daily_evolution_gift_binding (
    window_gift_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    window_id           INTEGER NOT NULL REFERENCES daily_evolution_window(window_id) ON DELETE CASCADE,
    gift_bundle_id      INTEGER NOT NULL REFERENCES evolution_gift_bundle(gift_bundle_id) ON DELETE CASCADE,
    roh_guard_kind_id   INTEGER NOT NULL REFERENCES roh_guard_kind(roh_guard_kind_id) ON DELETE RESTRICT,
    applied             INTEGER NOT NULL CHECK (applied IN (0,1)),
    r_delta             REAL NOT NULL,
    eco_delta           REAL NOT NULL,
    created_utc         TEXT NOT NULL,
    updated_utc         TEXT NOT NULL,
    CHECK (r_delta >= 0.0),
    CHECK (eco_delta >= 0.0)
);

-------------------------------------------------------------------------------
-- 9. RoH guard attachment for nanoimmune and lifeforce traits
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS roh_guard_attachment (
    roh_attachment_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    host_did            TEXT NOT NULL,
    roh_guard_kind_id   INTEGER NOT NULL REFERENCES roh_guard_kind(roh_guard_kind_id) ON DELETE RESTRICT,
    nanoimmune_id       INTEGER REFERENCES nanoimmune_object(nanoimmune_id) ON DELETE CASCADE,
    gift_bundle_id      INTEGER REFERENCES evolution_gift_bundle(gift_bundle_id) ON DELETE CASCADE,
    session_id          INTEGER NOT NULL REFERENCES session_envelope(session_id) ON DELETE CASCADE,
    roh_risk_before     REAL NOT NULL,
    roh_risk_after      REAL NOT NULL,
    lifeforce_band_code TEXT NOT NULL,  -- e.g. LIFEFORCE_BAND_SAFE
    guard_ok            INTEGER NOT NULL CHECK (guard_ok IN (0,1)),
    created_utc         TEXT NOT NULL,
    updated_utc         TEXT NOT NULL,
    CHECK (roh_risk_after <= roh_risk_before)
);

-------------------------------------------------------------------------------
-- 10. SQLite wiring index for AI/agent discovery
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS vampiric_healthcare_file_index (
    file_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    filename      TEXT NOT NULL,
    destination   TEXT NOT NULL,
    repotarget    TEXT NOT NULL,  -- e.g. Eco-Fort
    role          TEXT NOT NULL,  -- e.g. SQLSCHEMA
    description   TEXT NOT NULL,
    created_utc   TEXT NOT NULL
);

INSERT INTO vampiric_healthcare_file_index
    (filename, destination, repotarget, role, description, created_utc)
VALUES
    ('db_vampiric_healthcare_lifeforce.sql',
     'Eco-Fort/db/db_vampiric_healthcare_lifeforce.sql',
     'Eco-Fort',
     'SQLSCHEMA',
     'Vampiric-healthcare lifeforce schema: evolution gift bundles (10 slots), thermodynamic envelopes, nanoimmune objects, pain/fear boundaries, eco-repair transducers, neurovascular optimizer, daily loop bindings, and RoH guards.',
     datetime('now'));
