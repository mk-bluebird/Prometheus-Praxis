-- filename: Eco-Fort/db/phoenix_hex_registry.sql
-- destination: Eco-Fort/db/phoenix_hex_registry.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
--
-- Purpose:
--   Canonical, non-actuating registry for Phoenix hex anchors plus their
--   file- and particle-bindings, aligned with the Eco-Fort / EcoNet
--   governance spine and discovery spine.[file:34][file:36]
--
--   This DB is the single source of truth for:
--     - Hex anchors used in Phoenix eco-restoration work.
--     - File placement and discovery for hex-stamped artifacts.
--     - ALN particle bindings and evidencehex usage.[file:10][file:36]
--
--   All tables are append-only at the logical level (no destructive
--   updates in normal workflows). CI and agents MUST treat this DB as
--   read-only at runtime.[file:34][file:36]

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Phoenix hex anchors (canonical list)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS phoenix_hex_anchor (
    anchor_id        INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Stable logical name, unique across the constellation.
    -- Example: 'PHX_DRAINAGE_DECAY_20260708'.
    logical_name     TEXT NOT NULL UNIQUE,

    -- Hex string with 0x prefix, globally unique within this registry.
    -- Example: '0x20260708PHX3345NDrainageDecayBODTSSCEC'.[file:2]
    evidence_hex     TEXT NOT NULL UNIQUE,

    -- High-level domain for this anchor (HYDRO, CYBOQUATIC, MATERIAL, GOV, etc.).[file:36]
    domain           TEXT NOT NULL,

    -- Narrower sub-domain or corridor (e.g., DRAINAGE_DECAY, WORKLOAD_ENERGY_DV).[file:36]
    subdomain        TEXT NOT NULL,

    -- Region code, e.g. 'PHX-CAZ-CEIM'.[file:36]
    region_code      TEXT NOT NULL,

    -- Comma-separated planes this anchor touches:
    -- ENERGY,HYDRAULICS,BIODIVERSITY,MATERIALS,CARBON,DATA,TOPOLOGY,GOV.[file:36]
    planes           TEXT NOT NULL,

    -- Date label for daily progress anchors, yyyymmdd; NULL for timeless.[file:36]
    yyyymmdd         TEXT,

    -- Optional pointer to prior anchor_id for forward-only chains.
    prior_anchor_id  INTEGER,

    -- Bostrom DID / ALN identity binding.
    -- Example: 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'.[file:34][file:36]
    signing_did      TEXT NOT NULL,

    -- Short summary (≤ 256 chars).
    summary          TEXT NOT NULL,

    -- Extended description / research rationale.[file:2][file:36]
    description      TEXT,

    -- File-class hint: 'ALN','SQL','RUST','CPP','DOC','MIXED'.
    file_class       TEXT NOT NULL,

    -- Default root path for files using this anchor, repo-relative.
    -- Example: 'eco_restoration_shard/cyboquatic_progress/20260708'.[file:2]
    default_relpath  TEXT NOT NULL,

    -- Creation timestamp, UTC ISO-8601 string.[file:34]
    created_utc      TEXT NOT NULL,

    -- Activity flag; anchors remain immutable even when inactive.
    active           INTEGER NOT NULL DEFAULT 1
                       CHECK (active IN (0,1)),

    FOREIGN KEY (prior_anchor_id)
        REFERENCES phoenix_hex_anchor(anchor_id)
        ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_phx_hex_domain_date
    ON phoenix_hex_anchor (domain, subdomain, region_code, yyyymmdd);

----------------------------------------------------------------------
-- 2. File bindings (hex -> files)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS phoenix_hex_file (
    file_id          INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Anchor this file belongs to.
    anchor_id        INTEGER NOT NULL,

    -- Repo-relative path, e.g. 'eco_restoration_shard/cyboquatic_progress/20260708/sql/daily_progress_seed.sql'.[file:2]
    relpath          TEXT NOT NULL,

    -- Basename of the file, e.g. 'daily_progress_seed.sql'.
    filename         TEXT NOT NULL,

    -- File type: 'ALN','SQL','RUST','CPP','DOC','MIXED','OTHER'.
    file_type        TEXT NOT NULL,

    -- Hash of the file contents (consistent with Eco-Fort hash policy).
    -- Use the same allowed primitive used in definitionregistry/ artifactregistry.[file:34]
    file_hash_hex    TEXT NOT NULL,

    -- Logical scope: 'PARTICLE','MIGRATION','CRATE','DOC','MANIFEST','REGISTRY'.
    scope            TEXT NOT NULL,

    -- Optional link into definitionregistry (defid) or artifactregistry (artifact_id).[file:34]
    defid            TEXT,
    artifact_id      INTEGER,

    -- Creation / registration timestamp.
    created_utc      TEXT NOT NULL,

    FOREIGN KEY (anchor_id)
        REFERENCES phoenix_hex_anchor(anchor_id)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_phx_hex_file_anchor
    ON phoenix_hex_file (anchor_id);

CREATE INDEX IF NOT EXISTS idx_phx_hex_file_relpath
    ON phoenix_hex_file (relpath);

----------------------------------------------------------------------
-- 3. ALN particle bindings (hex -> ALN particles + evidence tables)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS phoenix_hex_particle_binding (
    binding_id       INTEGER PRIMARY KEY AUTOINCREMENT,

    anchor_id        INTEGER NOT NULL,

    -- ALN particle name, e.g. 'DrainageDecayFrame20260708v1'.
    particle_name    TEXT NOT NULL,

    -- ALN file path (repo-relative).
    particle_relpath TEXT NOT NULL,

    -- Particle role: 'GOVERNANCE','KER_KERNEL','DRAINAGE_DECAY','WORKLOAD','TRAY', etc.[file:10][file:31][file:36]
    particle_role    TEXT NOT NULL,

    -- Optional evidence table and column where this evidence_hex is stored, e.g. 'TrayBiodegradationLab2026v1','evidencehex'.[file:10]
    evidence_table   TEXT,
    evidence_column  TEXT,

    -- Extended notes describing how this binding is used.[file:10][file:36]
    notes            TEXT,

    created_utc      TEXT NOT NULL,

    FOREIGN KEY (anchor_id)
        REFERENCES phoenix_hex_anchor(anchor_id)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_phx_hex_particle_anchor
    ON phoenix_hex_particle_binding (anchor_id);

----------------------------------------------------------------------
-- 4. Agent / AI-facing view (joined surface)
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_phx_hex_registry AS
SELECT
    a.anchor_id,
    a.logical_name,
    a.evidence_hex,
    a.domain,
    a.subdomain,
    a.region_code,
    a.planes,
    a.yyyymmdd,
    a.prior_anchor_id,
    a.signing_did,
    a.summary,
    a.description,
    a.file_class,
    a.default_relpath,
    a.created_utc,
    a.active,
    f.file_id,
    f.relpath,
    f.filename,
    f.file_type,
    f.scope,
    p.binding_id,
    p.particle_name,
    p.particle_relpath,
    p.particle_role,
    p.evidence_table,
    p.evidence_column
FROM phoenix_hex_anchor AS a
LEFT JOIN phoenix_hex_file AS f
    ON f.anchor_id = a.anchor_id
LEFT JOIN phoenix_hex_particle_binding AS p
    ON p.anchor_id = a.anchor_id;

----------------------------------------------------------------------
-- 5. Seed anchors for existing Phoenix hex proofs
--
-- These rows encode anchors already present in your shards and
-- proofs (drainage-decay and workload-energy ΔVt).[file:2][file:31][file:36]
-- Adjust created_utc as needed when you first apply this script.
----------------------------------------------------------------------

INSERT OR IGNORE INTO phoenix_hex_anchor (
    logical_name,
    evidence_hex,
    domain,
    subdomain,
    region_code,
    planes,
    yyyymmdd,
    prior_anchor_id,
    signing_did,
    summary,
    description,
    file_class,
    default_relpath,
    created_utc,
    active
) VALUES
-- 1. Drainage-decay daily corridor (2026-07-08).[file:2]
(
    'PHX_DRAINAGE_DECAY_20260708',
    '0x20260708PHX3345NDrainageDecayBODTSSCEC',
    'HYDRO',
    'DRAINAGE_DECAY',
    'PHX-CAZ-CEIM',
    'HYDRAULICS,ENERGY,DATA',
    '20260708',
    NULL,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'Phoenix cyboquatic drainage decay frame for BOD/TSS/CEC corridors.',
    'Daily lyapunov-bounded drainage decay frame tying BOD, TSS, and CEC to CEIM windows and Vt residuals in Phoenix cyboquatic work.',
    'MIXED',
    'eco_restoration_shard/cyboquatic_progress/20260708',
    '2026-07-08T00:00:00Z',
    1
),
-- 2. Workload-energy ΔVt daily corridor (2026-07-09).[file:1][file:2]
(
    'PHX_WORKLOAD_ENERGY_DV_20260709',
    '0x20260709PHX3345NWorkloadEnergyDeltaVt',
    'CYBOQUATIC',
    'WORKLOAD_ENERGY_DV',
    'PHX-CAZ-CEIM',
    'ENERGY,HYDRAULICS,DATA',
    '20260709',
    NULL,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'Phoenix cyboquatic workload energy ΔVt daily progress anchor.',
    'Daily energy and drainage workload corridor for cyboquatic CF1-TRP nodes, binding telemetry workloads to Vt monotonicity and KER constraints in Phoenix.',
    'MIXED',
    'eco_restoration_shard/cyboquatic_progress/20260709',
    '2026-07-09T00:00:00Z',
    1
);

----------------------------------------------------------------------
-- 6. Seed file bindings for the two anchors (paths illustrative)
--
-- These should be kept in sync with concrete files in the mono-repo.
-- Hashes are placeholders: recompute with your allowed hashing primitive
-- (e.g., via a small Rust tool in Eco-Fort) and update.[file:34][file:36]
----------------------------------------------------------------------

INSERT OR IGNORE INTO phoenix_hex_file (
    anchor_id,
    relpath,
    filename,
    file_type,
    file_hash_hex,
    scope,
    defid,
    artifact_id,
    created_utc
)
SELECT
    a.anchor_id,
    'eco_restoration_shard/cyboquatic_progress/20260708/sql/daily_drainage_decay_seed.sql',
    'daily_drainage_decay_seed.sql',
    'SQL',
    '0xPHXSEEDHASH20260708',  -- replace via defreg-verify-style tool.[file:34]
    'MIGRATION',
    NULL,
    NULL,
    '2026-07-08T00:00:00Z'
FROM phoenix_hex_anchor AS a
WHERE a.logical_name = 'PHX_DRAINAGE_DECAY_20260708';

INSERT OR IGNORE INTO phoenix_hex_file (
    anchor_id,
    relpath,
    filename,
    file_type,
    file_hash_hex,
    scope,
    defid,
    artifact_id,
    created_utc
)
SELECT
    a.anchor_id,
    'eco_restoration_shard/cyboquatic_progress/20260709/sql/daily_workload_energy_dv_seed.sql',
    'daily_workload_energy_dv_seed.sql',
    'SQL',
    '0xPHXSEEDHASH20260709',  -- replace via defreg-verify-style tool.[file:34]
    'MIGRATION',
    NULL,
    NULL,
    '2026-07-09T00:00:00Z'
FROM phoenix_hex_anchor AS a
WHERE a.logical_name = 'PHX_WORKLOAD_ENERGY_DV_20260709';

----------------------------------------------------------------------
-- 7. Seed ALN particle bindings (illustrative)
--
-- These reference the ALN particles that will carry evidencehex fields
-- tied to the anchors (names follow your shard conventions).[file:10][file:35]
----------------------------------------------------------------------

INSERT OR IGNORE INTO phoenix_hex_particle_binding (
    anchor_id,
    particle_name,
    particle_relpath,
    particle_role,
    evidence_table,
    evidence_column,
    notes,
    created_utc
)
SELECT
    a.anchor_id,
    'DrainageDecayFrame20260708v1',
    'eco_restoration_shard/cyboquatic_progress/20260708/aln/DrainageDecayFrame20260708v1.aln',
    'DRAINAGE_DECAY',
    'CyboquaticDrainageDecay20260708v1',
    'evidencehex',
    'Daily Phoenix drainage decay frame with CEIM-derived KER and Vt residual.',
    '2026-07-08T00:00:00Z'
FROM phoenix_hex_anchor AS a
WHERE a.logical_name = 'PHX_DRAINAGE_DECAY_20260708';

INSERT OR IGNORE INTO phoenix_hex_particle_binding (
    anchor_id,
    particle_name,
    particle_relpath,
    particle_role,
    evidence_table,
    evidence_column,
    notes,
    created_utc
)
SELECT
    a.anchor_id,
    'WorkloadEnergyDeltaVt20260709v1',
    'eco_restoration_shard/cyboquatic_progress/20260709/aln/WorkloadEnergyDeltaVt20260709v1.aln',
    'WORKLOAD',
    'CyboquaticWorkloadEnergyDeltaVt20260709v1',
    'evidencehex',
    'Daily Phoenix cyboquatic workload energy ΔVt frame with KER-compliant residuals.',
    '2026-07-09T00:00:00Z'
FROM phoenix_hex_anchor AS a
WHERE a.logical_name = 'PHX_WORKLOAD_ENERGY_DV_20260709';

----------------------------------------------------------------------
-- 8. Helper view: missing bindings (for CI)
--
-- v_phx_hex_missing_file:
--   anchors with no associated phoenix_hex_file rows.
--
-- v_phx_hex_missing_particle:
--   anchors with no associated phoenix_hex_particle_binding rows.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_phx_hex_missing_file AS
SELECT
    a.anchor_id,
    a.logical_name,
    a.evidence_hex,
    a.domain,
    a.subdomain,
    a.region_code,
    a.planes,
    a.yyyymmdd
FROM phoenix_hex_anchor AS a
LEFT JOIN phoenix_hex_file AS f
    ON f.anchor_id = a.anchor_id
WHERE f.file_id IS NULL;

CREATE VIEW IF NOT EXISTS v_phx_hex_missing_particle AS
SELECT
    a.anchor_id,
    a.logical_name,
    a.evidence_hex,
    a.domain,
    a.subdomain,
    a.region_code,
    a.planes,
    a.yyyymmdd
FROM phoenix_hex_anchor AS a
LEFT JOIN phoenix_hex_particle_binding AS p
    ON p.anchor_id = a.anchor_id
WHERE p.binding_id IS NULL;
