-- filename: db_blocktime_finality_planetaryhealth.sql
-- destination: ecorestorationshard/db/db_blocktime_finality_planetaryhealth.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------
-- 1. Lane / transaction-class block timing policy
-------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS txclass_blockpolicy (
    policy_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    region           TEXT NOT NULL,           -- e.g. "Phoenix-AZ"
    lane             TEXT NOT NULL,           -- e.g. "PROD", "RESEARCH"
    tx_class         TEXT NOT NULL,           -- "ECO", "REGULAR"
    block_time_sec   INTEGER NOT NULL,        -- target block time in seconds
    finality_blocks  INTEGER NOT NULL,        -- blocks to consider final
    active           INTEGER NOT NULL DEFAULT 1,
    author_bostrom   TEXT NOT NULL,
    author_contract  TEXT NOT NULL,
    created_utc      TEXT NOT NULL,
    updated_utc      TEXT NOT NULL,
    UNIQUE(region, lane, tx_class)
);

-- Seed example for Phoenix:
-- ECO messages: 3s blocks, 2-block finality (≈6s)
-- REGULAR tx: 6s blocks, (optionally) 2-block finality (≈12s)
INSERT OR IGNORE INTO txclass_blockpolicy (
    region, lane, tx_class, block_time_sec, finality_blocks,
    active, author_bostrom, author_contract, created_utc, updated_utc
) VALUES
    ('Phoenix-AZ', 'PROD', 'ECO',      3, 2, 1,
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     'LaneTimingPhoenix2026v1',
     datetime('now'), datetime('now')),
    ('Phoenix-AZ', 'PROD', 'REGULAR',  6, 2, 1,
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     'LaneTimingPhoenix2026v1',
     datetime('now'), datetime('now'));

-------------------------------------------------------------------
-- 2. Consensus scheduler view: resolves effective timing per-class
-------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_blocktime_effective AS
SELECT
    region,
    lane,
    tx_class,
    block_time_sec,
    finality_blocks,
    (block_time_sec * finality_blocks) AS finality_sec,
    active,
    author_bostrom,
    author_contract
FROM txclass_blockpolicy
WHERE active = 1;

-------------------------------------------------------------------
-- 3. BCI challenge visual cue schema for bostrom-signer
-------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS bostrom_bci_challenge (
    challenge_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    tx_hash               TEXT NOT NULL,
    region                TEXT NOT NULL,
    eco_co2_kg            REAL NOT NULL,         -- estimated CO₂ reduced
    eco_cost_boot         REAL NOT NULL,         -- cost in boot
    eco_wealth_delta      REAL NOT NULL,         -- eco-wealth points
    visual_cue            TEXT NOT NULL,         -- rendered summary string
    bci_challenge_version TEXT NOT NULL,         -- e.g. "BCIChallenge2026v1"
    created_utc           TEXT NOT NULL
);

-- Helper view: canonical visual cue format for UI
CREATE VIEW IF NOT EXISTS v_bci_visual_cue AS
SELECT
    challenge_id,
    tx_hash,
    printf(
        'This action will reduce CO₂ by %.3f kg, cost %.3f boot, and credit your eco-wealth score by %.3f points',
        eco_co2_kg,
        eco_cost_boot,
        eco_wealth_delta
    ) AS visual_cue,
    bci_challenge_version,
    created_utc
FROM bostrom_bci_challenge;

-------------------------------------------------------------------
-- 4. PlanetaryHealthIndex and ROD_ROH_MAX evolution wiring
-------------------------------------------------------------------

-- PlanetaryHealthIndex snapshot per evolution window (0–1)
CREATE TABLE IF NOT EXISTS planetary_health_index (
    phi_id           INTEGER PRIMARY KEY AUTOINCREMENT,
    region           TEXT NOT NULL,          -- "GLOBAL" or specific region
    window_start_utc TEXT NOT NULL,
    window_end_utc   TEXT NOT NULL,
    index_value      REAL NOT NULL,         -- 0.0 – 1.0
    source_contract  TEXT NOT NULL,         -- e.g. "PlanetaryHealthIndex2026v1"
    author_bostrom   TEXT NOT NULL,
    created_utc      TEXT NOT NULL
);

-- ROD_ROH_MAX evolution log, derived from PlanetaryHealthIndex
-- Formula (normalized to ALN-safe integers externally):
--   rod_roh_max = 0.30 * (1 + (1 - index) * 0.2)
--               = 0.30 * (1.0 + 0.2 - 0.2*index)
--               = 0.36 - 0.06 * index
CREATE TABLE IF NOT EXISTS rod_roh_max_evolution (
    rod_id           INTEGER PRIMARY KEY AUTOINCREMENT,
    region           TEXT NOT NULL,          -- usually "GLOBAL"
    window_start_utc TEXT NOT NULL,
    window_end_utc   TEXT NOT NULL,
    phi_index_value  REAL NOT NULL,
    rod_roh_max      REAL NOT NULL,         -- continuous value in 0–1
    rod_roh_max_int  INTEGER NOT NULL,      -- fixed-point, e.g. value * 10000
    author_bostrom   TEXT NOT NULL,
    author_contract  TEXT NOT NULL,
    kani_verified    INTEGER NOT NULL DEFAULT 0, -- 1 when Kani harness passes
    kani_run_id      TEXT,                  -- identifier of Kani test run
    created_utc      TEXT NOT NULL
);

-- View joining phi and ROD_ROH_MAX for evolution diagnostics
CREATE VIEW IF NOT EXISTS v_rod_roh_max_with_phi AS
SELECT
    r.rod_id,
    r.region,
    r.window_start_utc,
    r.window_end_utc,
    r.phi_index_value,
    r.rod_roh_max,
    r.rod_roh_max_int,
    r.kani_verified,
    r.kani_run_id,
    r.author_bostrom,
    r.author_contract,
    p.source_contract AS phi_source_contract
FROM rod_roh_max_evolution r
JOIN planetary_health_index p
  ON p.region = r.region
 AND p.window_start_utc = r.window_start_utc
 AND p.window_end_utc   = r.window_end_utc;

-------------------------------------------------------------------
-- 5. ALN particle sketch for evolution broadcast (stored as text)
-------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS aln_daily_evolution_particle (
    particle_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    region           TEXT NOT NULL,
    window_start_utc TEXT NOT NULL,
    window_end_utc   TEXT NOT NULL,
    aln_payload      TEXT NOT NULL,  -- serialized ALN particle text
    author_bostrom   TEXT NOT NULL,
    author_contract  TEXT NOT NULL,
    created_utc      TEXT NOT NULL
);

-- Example ALN payload pattern (non-enforced here, for documentation):
-- 
--   particle PlanetaryHealthEvo2026v1 {
--       region: "GLOBAL",
--       window_start_utc: "...",
--       window_end_utc:   "...",
--       PlanetaryHealthIndex: 0.87,
--       ROD_ROH_MAX: 0.36 - 0.06 * 0.87
--   }
--
-- Kani harness must:
--   - parse aln_payload
--   - recompute ROD_ROH_MAX from PlanetaryHealthIndex
--   - compare to rod_roh_max_int in rod_roh_max_evolution (within epsilon)
--   - set kani_verified = 1 on success.
