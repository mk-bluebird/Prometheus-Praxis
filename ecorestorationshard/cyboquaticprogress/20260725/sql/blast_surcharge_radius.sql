-- filename: ecorestorationshard/cyboquaticprogress/20260725/sql/blast_surcharge_radius.sql
-- destination: ecorestorationshard/cyboquaticprogress/20260725/sql/blast_surcharge_radius.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Blast-radius diagnostic table for surcharge breaches
--    Non-actuating, append-only telemetry for blast radius frames.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS blast_surcharge_radius (
    diag_id              INTEGER PRIMARY KEY AUTOINCREMENT,
    -- Hex-anchored canal node and FOG region/channel identifiers
    canal_node_id        TEXT    NOT NULL,
    fog_region_id        TEXT    NOT NULL,
    fog_channel_id       TEXT    NOT NULL,

    -- Time of diagnostic (UTC ISO-8601 string)
    timestamputc         TEXT    NOT NULL,

    -- Physical metrics (normalized or raw, corridor-bounded upstream)
    surcharge_index      REAL    NOT NULL,  -- dimensionless, 0..1 corridor [file:2]
    flow_m3s             REAL    NOT NULL,  -- flow at breach segment [file:2]
    head_loss_m          REAL    NOT NULL,  -- local head loss [file:2]
    bulk_density_kg_m3   REAL    NOT NULL,  -- water+sediment effective density [file:2]
    energy_j             REAL    NOT NULL,  -- estimated energy involved in surcharge [file:2]

    -- Output of C++ diagnostic kernel
    blast_radius_m       REAL    NOT NULL,  -- predicted radial extent of surcharge impact [file:2]
    vt_residual          REAL    NOT NULL,  -- Lyapunov residual slice 0..1 [file:2]
    r_energy             REAL    NOT NULL,  -- risk coordinate 0..1 [file:2]
    r_hydraulics         REAL    NOT NULL,  -- risk coordinate 0..1 [file:2]
    r_bio                REAL    NOT NULL,  -- risk coordinate 0..1 [file:2]
    r_tox                REAL    NOT NULL,  -- risk coordinate 0..1 [file:2]
    r_uncertainty        REAL    NOT NULL,  -- data quality / sensor health plane [file:2]
    r_topology           REAL    NOT NULL,  -- topology risk coordinate (graph anomalies) [file:2]

    -- K,E,R triad and composite score
    k_knowledge          REAL    NOT NULL,
    e_ecoimpact          REAL    NOT NULL,
    r_risk               REAL    NOT NULL,
    kerscore             REAL    NOT NULL,

    -- Governance bindings
    lane                 TEXT    NOT NULL,  -- RESEARCH | PILOT | PRODUCTION [file:7]
    governance_particle  TEXT    NOT NULL,  -- ALN particle name
    evidence_hex         TEXT    NOT NULL,  -- Phoenix evidence hex, bound in registry [file:11]
    signing_did          TEXT    NOT NULL   -- e.g. bostrom18... [file:11]
);

----------------------------------------------------------------------
-- 2. Domain-specific constraints and invariants
--    These CHECKs keep values corridor-bounded and non-actuating.
----------------------------------------------------------------------

ALTER TABLE blast_surcharge_radius
    ADD COLUMN IF NOT EXISTS vt_band TEXT DEFAULT 'BLASTRADIUS' NOT NULL;

CREATE TABLE IF NOT EXISTS blast_surcharge_radius_invariants (
    invariant_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    name           TEXT NOT NULL UNIQUE,
    description    TEXT NOT NULL
);

INSERT OR IGNORE INTO blast_surcharge_radius_invariants (name, description) VALUES
    ('value_bounds',
     'Ensure risk coordinates and K,E,R in 0..1, blast radius >= 0, surcharge_index in 0..1.'),
    ('non_actuating',
     'Table is telemetry-only; no actuator references or commands are stored.'),
    ('hex_bound',
     'Each row must carry a Phoenix evidence_hex registered in Eco-Fort dbphoenixhexregistry.sql.');

----------------------------------------------------------------------
-- 3. CHECK constraints via triggers (SQLite lacks row-level CHECK on existing rows)
--    BEFORE INSERT / UPDATE triggers enforce invariants on new rows.
----------------------------------------------------------------------

DROP TRIGGER IF EXISTS trg_blast_surcharge_radius_bounds;

CREATE TRIGGER trg_blast_surcharge_radius_bounds
BEFORE INSERT ON blast_surcharge_radius
BEGIN
    -- Risk coordinates and K,E,R bounds 0..1
    SELECT
        CASE
            WHEN NEW.r_energy      < 0.0 OR NEW.r_energy      > 1.0
              OR NEW.r_hydraulics  < 0.0 OR NEW.r_hydraulics  > 1.0
              OR NEW.r_bio         < 0.0 OR NEW.r_bio         > 1.0
              OR NEW.r_tox         < 0.0 OR NEW.r_tox         > 1.0
              OR NEW.r_uncertainty < 0.0 OR NEW.r_uncertainty > 1.0
              OR NEW.r_topology    < 0.0 OR NEW.r_topology    > 1.0
              OR NEW.k_knowledge   < 0.0 OR NEW.k_knowledge   > 1.0
              OR NEW.e_ecoimpact   < 0.0 OR NEW.e_ecoimpact   > 1.0
              OR NEW.r_risk        < 0.0 OR NEW.r_risk        > 1.0
              OR NEW.kerscore      < 0.0 OR NEW.kerscore      > 1.0
              OR NEW.vt_residual   < 0.0 OR NEW.vt_residual   > 1.0
              OR NEW.surcharge_index < 0.0 OR NEW.surcharge_index > 1.0
              OR NEW.blast_radius_m < 0.0
            THEN RAISE(ABORT, 'blast_surcharge_radius: invariant violation (bounds)')
        END;
END;

DROP TRIGGER IF EXISTS trg_blast_surcharge_radius_lane;

CREATE TRIGGER trg_blast_surcharge_radius_lane
BEFORE INSERT ON blast_surcharge_radius
BEGIN
    -- Lane must be one of RESEARCH, PILOT, PRODUCTION
    SELECT
        CASE
            WHEN NEW.lane NOT IN ('RESEARCH', 'PILOT', 'PRODUCTION')
            THEN RAISE(ABORT, 'blast_surcharge_radius: invalid lane')
        END;
END;

----------------------------------------------------------------------
-- 4. Indices for energy-efficient queries (range scans on time/FoG/canal)
----------------------------------------------------------------------

CREATE INDEX IF NOT EXISTS idx_blast_surcharge_time
    ON blast_surcharge_radius (timestamputc);

CREATE INDEX IF NOT EXISTS idx_blast_surcharge_fog_time
    ON blast_surcharge_radius (fog_region_id, timestamputc);

CREATE INDEX IF NOT EXISTS idx_blast_surcharge_canal_time
    ON blast_surcharge_radius (canal_node_id, timestamputc);

CREATE INDEX IF NOT EXISTS idx_blast_surcharge_radius_risk
    ON blast_surcharge_radius (blast_radius_m, vt_residual, r_risk);

----------------------------------------------------------------------
-- 5. Daily progress linkage (seed row for 2026-07-25 in shared ledger)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS dailyprogress (
    progress_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    yyyymmdd         TEXT    NOT NULL,
    domain           TEXT    NOT NULL,
    subdomain        TEXT    NOT NULL,
    subtask_id       TEXT    NOT NULL,
    k_knowledge      REAL    NOT NULL,
    e_ecoimpact      REAL    NOT NULL,
    r_risk           REAL    NOT NULL,
    vt_residual      REAL    NOT NULL,
    evidence_hex     TEXT    NOT NULL,
    prior_pointer    TEXT,
    signing_did      TEXT    NOT NULL
);

INSERT OR IGNORE INTO dailyprogress (
    yyyymmdd, domain, subdomain, subtask_id,
    k_knowledge, e_ecoimpact, r_risk, vt_residual,
    evidence_hex, prior_pointer, signing_did
) VALUES (
    '20260725',
    'CYBOQUATIC',
    'BLASTRADIUS_SURCHARGE',
    'PHX-CANAL-BR-2026-07-25',
    0.90,   -- placeholder corridor-consistent K [file:2]
    0.88,   -- placeholder E [file:2]
    0.15,   -- placeholder R [file:2]
    0.40,   -- placeholder Vt [file:2]
    '0x20260725PHXBLASTRADIUS_SURCHARGE', -- new hex, must be registered in phoenixhexanchor [file:11]
    '0x20260709PHX3345NWorkloadEnergyDeltaVt', -- prior chain pointer to workload day [file:11]
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);
