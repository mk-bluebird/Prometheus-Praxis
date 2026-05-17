-- filename: db_vampiric_healthcare_traits.sql
-- destination: eco_restoration_shard/sql/health/db_vampiric_healthcare_traits.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. EvolutionGiftBundle registry (10-slot forward-only bundles)
--    Each bundle is a forward-only, non-revocable gift set bound to
--    a thermodynamic envelope, RoH guard profile, and continuity anchor.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS evolution_gift_bundle (
    bundle_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    bundle_code      TEXT NOT NULL UNIQUE,      -- e.g. "EGB-LIFE-RES-001"
    description      TEXT NOT NULL,

    -- Thermodynamic envelope binding: which host-level envelope
    -- and corridor polytope this bundle is valid under.
    thermodynamic_envelope_id TEXT NOT NULL,
    roh_profile_id            TEXT NOT NULL,    -- RoH / RoD / lane profile

    -- Psychological continuity anchor: references a continuity shard
    -- or host-governance profile that must hold across sessions.
    continuity_anchor_id TEXT NOT NULL,

    -- 10-slot capacity invariant: enforced at slot table level.
    max_slots           INTEGER NOT NULL DEFAULT 10 CHECK (max_slots = 10),

    -- Forward-only invariant: bundles cannot be downgraded or rolled back.
    forward_only        INTEGER NOT NULL DEFAULT 1 CHECK (forward_only IN (0,1)),

    created_utc         TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc         TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TRIGGER IF NOT EXISTS trg_evolution_gift_bundle_updated
AFTER UPDATE ON evolution_gift_bundle
BEGIN
    UPDATE evolution_gift_bundle
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE bundle_id = NEW.bundle_id;
END;

----------------------------------------------------------------------
-- 2. Gift slots inside a bundle
--    10 forward-only slots per bundle; each slot binds to a trait
--    and to corridor/guard constraints.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS evolution_gift_slot (
    slot_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    bundle_id    INTEGER NOT NULL REFERENCES evolution_gift_bundle(bundle_id) ON DELETE CASCADE,
    slot_index   INTEGER NOT NULL,        -- 1..10
    trait_id     INTEGER NOT NULL,        -- FK to lifeforce_trait.trait_id
    active       INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),

    -- Forward-only constraint: once activated and confirmed, cannot be cleared.
    locked       INTEGER NOT NULL DEFAULT 0 CHECK (locked IN (0,1)),

    -- Guard profile binding for this slot: which RoH / ThermodynamicEnvelope
    -- configuration must be satisfied for the associated trait to apply.
    guard_profile_id TEXT NOT NULL,

    created_utc  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),

    UNIQUE (bundle_id, slot_index)
);

CREATE TRIGGER IF NOT EXISTS trg_evolution_gift_slot_updated
AFTER UPDATE ON evolution_gift_slot
BEGIN
    UPDATE evolution_gift_slot
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE slot_id = NEW.slot_id;
END;

-- Enforce 10-slot capacity per bundle (CI can assert slot_index in 1..10).
CREATE INDEX IF NOT EXISTS idx_evolution_gift_slot_bundle
    ON evolution_gift_slot (bundle_id, slot_index);

----------------------------------------------------------------------
-- 3. Core lifeforce traits for vampiric healthcare
--    Includes LIFEFORCE_RESILIENCE_REGENERATOR and related traits.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS lifeforce_trait (
    trait_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    trait_code    TEXT NOT NULL UNIQUE,  -- e.g. "LIFEFORCE_RESILIENCE_REGENERATOR"
    display_name  TEXT NOT NULL,
    description   TEXT NOT NULL,

    -- Thermodynamic envelope binding: which envelope this trait assumes.
    thermodynamic_envelope_id TEXT NOT NULL,

    -- Guard hook: name of guard kernel / ALN particle that must sign off
    -- (e.g. LifeforceEnvelopeGuard, NeuroThermoGuard).
    guard_kernel_code TEXT NOT NULL,

    -- Flags for daily loop / RoH integration.
    daily_loop_integrated INTEGER NOT NULL DEFAULT 0 CHECK (daily_loop_integrated IN (0,1)),
    roh_guard_required    INTEGER NOT NULL DEFAULT 1 CHECK (roh_guard_required IN (0,1)),

    created_utc    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TRIGGER IF NOT EXISTS trg_lifeforce_trait_updated
AFTER UPDATE ON lifeforce_trait
BEGIN
    UPDATE lifeforce_trait
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE trait_id = NEW.trait_id;
END;

----------------------------------------------------------------------
-- 4. Specialized trait metadata tables for the 41-60 family
--    These tables avoid overloading a single JSON blob and keep
--    each physiologic corridor queryable and guardable.
----------------------------------------------------------------------

-- 4.1 Thermodynamic / nanoimmune envelope linkage for reusable nanoimmune objects.
CREATE TABLE IF NOT EXISTS nanoimmune_envelope (
    envelope_id   TEXT PRIMARY KEY,      -- stable ID referenced by lifeforce_trait
    description   TEXT NOT NULL,

    -- Link to ThermodynamicEnvelope corridor polytope (e.g. RoH plane ID).
    thermodynamic_envelope_id TEXT NOT NULL,

    -- Reusable nanoimmune object family tag.
    nanoimmune_family_code   TEXT NOT NULL,  -- e.g. "REUSABLE_NANOIMMUNE_V1"

    -- RoH ceiling and lifeforce floor for this envelope.
    roh_ceiling     REAL NOT NULL CHECK (roh_ceiling BETWEEN 0.0 AND 0.3),
    lifeforce_floor REAL NOT NULL CHECK (lifeforce_floor BETWEEN 0.0 AND 1.0),

    created_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TRIGGER IF NOT EXISTS trg_nanoimmune_envelope_updated
AFTER UPDATE ON nanoimmune_envelope
BEGIN
    UPDATE nanoimmune_envelope
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE envelope_id = NEW.envelope_id;
END;

-- 4.2 Fear/pain as protected boundaries (not r-inputs).
--     This table encodes which shards treat fear/pain as hard veto surfaces
--     rather than incremental risk coordinates.
CREATE TABLE IF NOT EXISTS fear_pain_boundary_policy (
    policy_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    policy_code      TEXT NOT NULL UNIQUE,   -- e.g. "FEAR_PAIN_VETO_V1",
    description      TEXT NOT NULL,

    -- Flags: fear/pain acts as hard veto vs soft weight.
    fear_as_veto     INTEGER NOT NULL DEFAULT 1 CHECK (fear_as_veto IN (0,1)),
    pain_as_veto     INTEGER NOT NULL DEFAULT 1 CHECK (pain_as_veto IN (0,1)),

    -- Guard kernels that must enforce this policy.
    roh_guard_kernel_code   TEXT NOT NULL,
    neurorights_guard_code  TEXT NOT NULL,

    created_utc      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TRIGGER IF NOT EXISTS trg_fear_pain_boundary_policy_updated
AFTER UPDATE ON fear_pain_boundary_policy
BEGIN
    UPDATE fear_pain_boundary_policy
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE policy_id = NEW.policy_id;
END;

-- 4.3 Psychological continuity anchors.
CREATE TABLE IF NOT EXISTS continuity_anchor (
    continuity_anchor_id TEXT PRIMARY KEY,
    description          TEXT NOT NULL,

    -- Links to host DID and RoH continuity profile.
    host_did             TEXT NOT NULL,
    continuity_profile_code TEXT NOT NULL,  -- e.g. "PSY_CONTINUITY_STRICT_V1",

    -- Minimum session spacing and allowable drifts (seconds / scalar).
    min_session_interval_secs INTEGER NOT NULL CHECK (min_session_interval_secs >= 0),
    max_identity_drift        REAL NOT NULL CHECK (max_identity_drift >= 0.0),

    created_utc          TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc          TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TRIGGER IF NOT EXISTS trg_continuity_anchor_updated
AFTER UPDATE ON continuity_anchor
BEGIN
    UPDATE continuity_anchor
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE continuity_anchor_id = NEW.continuity_anchor_id;
END;

----------------------------------------------------------------------
-- 5. Physiologic corridor traits (protein synthesis, neurovascular, eco-karma, digest regen)
----------------------------------------------------------------------

-- 5.1 Protein synthesis healing boost corridors.
CREATE TABLE IF NOT EXISTS protein_synthesis_corridor (
    corridor_id   TEXT PRIMARY KEY,
    description   TEXT NOT NULL,

    -- Normalized corridor parameters 0..1 for safety envelopes.
    max_daily_anabolic_load REAL NOT NULL CHECK (max_daily_anabolic_load BETWEEN 0.0 AND 1.0),
    min_rest_interval_secs   INTEGER NOT NULL CHECK (min_rest_interval_secs >= 0),

    -- Link to RoH plane / organ corridor (e.g. microvascular, hepatic).
    roh_plane_code   TEXT NOT NULL,

    created_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TRIGGER IF NOT EXISTS trg_protein_synthesis_corridor_updated
AFTER UPDATE ON protein_synthesis_corridor
BEGIN
    UPDATE protein_synthesis_corridor
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE corridor_id = NEW.corridor_id;
END;

-- 5.2 Neurovascular optimizer corridor.
CREATE TABLE IF NOT EXISTS neurovascular_optimizer_corridor (
    corridor_id   TEXT PRIMARY KEY,
    description   TEXT NOT NULL,

    -- Normalized bounds for cerebral perfusion and neurovascular load.
    max_neurovascular_load REAL NOT NULL CHECK (max_neurovascular_load BETWEEN 0.0 AND 1.0),
    min_hrv_band           REAL NOT NULL CHECK (min_hrv_band BETWEEN 0.0 AND 1.0),

    roh_plane_code   TEXT NOT NULL,

    created_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TRIGGER IF NOT EXISTS trg_neurovascular_optimizer_corridor_updated
AFTER UPDATE ON neurovascular_optimizer_corridor
BEGIN
    UPDATE neurovascular_optimizer_corridor
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE corridor_id = NEW.corridor_id;
END;

-- 5.3 Eco-repair karma drain transducer.
CREATE TABLE IF NOT EXISTS eco_repair_karma_transducer (
    transducer_id TEXT PRIMARY KEY,
    description   TEXT NOT NULL,

    -- Mapping between eco-repair work and karma drain units.
    eco_repair_unit      REAL NOT NULL CHECK (eco_repair_unit >= 0.0),
    karma_drain_per_unit REAL NOT NULL CHECK (karma_drain_per_unit >= 0.0),

    -- Floors and ceilings to prevent over-drain or free arbitrage.
    min_karma_floor REAL NOT NULL CHECK (min_karma_floor >= 0.0),
    max_karma_ceiling REAL NOT NULL CHECK (max_karma_ceiling >= 0.0),

    created_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TRIGGER IF NOT EXISTS trg_eco_repair_karma_transducer_updated
AFTER UPDATE ON eco_repair_karma_transducer
BEGIN
    UPDATE eco_repair_karma_transducer
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE transducer_id = NEW.transducer_id;
END;

-- 5.4 Pain micro-shield corridors.
CREATE TABLE IF NOT EXISTS pain_micro_shield_corridor (
    corridor_id   TEXT PRIMARY KEY,
    description   TEXT NOT NULL,

    -- Upper bound on allowed pain envelope (normalized 0..1).
    max_pain_envelope REAL NOT NULL CHECK (max_pain_envelope BETWEEN 0.0 AND 1.0),

    -- Time window in seconds over which micro-shield must dissipate.
    shield_window_secs INTEGER NOT NULL CHECK (shield_window_secs >= 0),

    created_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TRIGGER IF NOT EXISTS trg_pain_micro_shield_corridor_updated
AFTER UPDATE ON pain_micro_shield_corridor
BEGIN
    UPDATE pain_micro_shield_corridor
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE corridor_id = NEW.corridor_id;
END;

-- 5.5 Digest regen extractors.
CREATE TABLE IF NOT EXISTS digest_regen_extractor (
    extractor_id  TEXT PRIMARY KEY,
    description   TEXT NOT NULL,

    -- Digest load and regeneration parameters (normalized 0..1).
    max_digest_load     REAL NOT NULL CHECK (max_digest_load BETWEEN 0.0 AND 1.0),
    regen_bias_factor   REAL NOT NULL CHECK (regen_bias_factor >= 0.0),

    -- Link to digest corridor / RoH axis.
    digest_corridor_code TEXT NOT NULL,

    created_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

CREATE TRIGGER IF NOT EXISTS trg_digest_regen_extractor_updated
AFTER UPDATE ON digest_regen_extractor
BEGIN
    UPDATE digest_regen_extractor
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE extractor_id = NEW.extractor_id;
END;

----------------------------------------------------------------------
-- 6. Daily loop integration and RoH guard linkage
--    This table ties traits / bundles into daily loops and RoH guards.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS daily_loop_guard_binding (
    binding_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    host_did      TEXT NOT NULL,
    bundle_id     INTEGER NOT NULL REFERENCES evolution_gift_bundle(bundle_id) ON DELETE CASCADE,
    trait_id      INTEGER NOT NULL REFERENCES lifeforce_trait(trait_id) ON DELETE CASCADE,

    -- Daily loop identifier (e.g. morning/evening healthcare loop).
    daily_loop_code TEXT NOT NULL,        -- "DAILY_LOOP_HEALTHCARE_V1"

    -- RoH / RoD guard profile that must be applied before this trait/slot.
    roh_profile_id   TEXT NOT NULL,

    -- RoH guard status cache (non-authoritative; updated by guards).
    last_guard_status   TEXT NOT NULL,    -- "SAFE", "BRAKE", "HOLD"
    last_guard_checked_utc TEXT NOT NULL,

    created_utc   TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_utc   TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),

    UNIQUE (host_did, bundle_id, trait_id, daily_loop_code)
);

CREATE TRIGGER IF NOT EXISTS trg_daily_loop_guard_binding_updated
AFTER UPDATE ON daily_loop_guard_binding
BEGIN
    UPDATE daily_loop_guard_binding
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE binding_id = NEW.binding_id;
END;
