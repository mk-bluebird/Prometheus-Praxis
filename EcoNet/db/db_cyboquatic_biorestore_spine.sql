PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Cyboquatic biodegradable-material channels: governance-grade evidence
--    Non‑actuating, diagnostic only. All actuation stays in Rust/ALN kernels.
-------------------------------------------------------------------------------

-- Table: cybo_material_batch
-- Purpose:
-- - Register biodegradable substrate batches used in Cyboquatic machinery.
-- - Track composition, manufacturing route, and eco‑risk coordinates.
-- - All risk coordinates are normalized in [0,1] and match existing planes.
--
-- Planes:
-- - materials: rmass_loss, rresidue, rfragment
-- - carbon   : rcarbon_embodied
-- - water    : rleachate, rtox_aquatic
-- - bio      : rbiodiv
-- Encoding:
-- - Values are stored as REAL but must respect 0..1 and non‑offsettable planes.

CREATE TABLE IF NOT EXISTS cybo_material_batch (
    batch_id            TEXT PRIMARY KEY,           -- e.g. "BioSub-2026-03A"
    repo_name           TEXT NOT NULL,              -- e.g. "ecorestorationshard"
    material_family     TEXT NOT NULL,              -- e.g. "tray", "baffle"
    description         TEXT NOT NULL,
    manufacture_utc     TEXT NOT NULL,              -- ISO8601
    region_code         TEXT NOT NULL,              -- e.g. "Phoenix-AZ"
    process_route       TEXT NOT NULL,              -- freeform, non‑secret
    -- Normalized eco‑risk coordinates 0..1, must match existing plane IDs
    rmass_loss          REAL NOT NULL CHECK (rmass_loss BETWEEN 0.0 AND 1.0),
    rresidue            REAL NOT NULL CHECK (rresidue BETWEEN 0.0 AND 1.0),
    rfragment           REAL NOT NULL CHECK (rfragment BETWEEN 0.0 AND 1.0),
    rcarbon_embodied    REAL NOT NULL CHECK (rcarbon_embodied BETWEEN 0.0 AND 1.0),
    rleachate           REAL NOT NULL CHECK (rleachate BETWEEN 0.0 AND 1.0),
    rtox_aquatic        REAL NOT NULL CHECK (rtox_aquatic BETWEEN 0.0 AND 1.0),
    rbiodiv             REAL NOT NULL CHECK (rbiodiv BETWEEN 0.0 AND 1.0),
    -- Governance annotations
    lane_default        TEXT NOT NULL CHECK (
        lane_default IN ('RESEARCH','EXPPROD','PROD')
    ),
    ker_k_target        REAL NOT NULL CHECK (ker_k_target BETWEEN 0.0 AND 1.0),
    ker_e_target        REAL NOT NULL CHECK (ker_e_target BETWEEN 0.0 AND 1.0),
    ker_r_ceiling       REAL NOT NULL CHECK (ker_r_ceiling BETWEEN 0.0 AND 1.0),
    non_actuating_only  INTEGER NOT NULL CHECK (non_actuating_only IN (0,1)),
    did_owner           TEXT NOT NULL,
    signing_did         TEXT,
    evidence_hex        TEXT
);

CREATE INDEX IF NOT EXISTS idx_cybo_material_region
    ON cybo_material_batch (region_code, material_family);

CREATE INDEX IF NOT EXISTS idx_cybo_material_ker
    ON cybo_material_batch (lane_default, ker_r_ceiling, rmass_loss, rleachate);


-------------------------------------------------------------------------------
-- 2. Cyboquatic material deployment evidence (channels + nodes)
-------------------------------------------------------------------------------

-- Table: cybo_channel_material
-- Purpose:
-- - Log where and how biodegradable batches are deployed into Cyboquatic nodes.
-- - Connect batches to existing EcoNet node IDs and EcoNet channels.
-- - Provide a strictly evidence‑grade trace for MAR/FOG/SAT deployments.
--
-- Non‑actuating:
-- - These rows are written after physical deployment by audit/telemetry jobs.
-- - They never encode commands, setpoints, or routing decisions.

CREATE TABLE IF NOT EXISTS cybo_channel_material (
    deploy_id           INTEGER PRIMARY KEY AUTOINCREMENT,
    batch_id            TEXT NOT NULL REFERENCES cybo_material_batch(batch_id)
                           ON DELETE CASCADE,
    node_id             TEXT NOT NULL,        -- must align with existing node IDs
    channel_kind        TEXT NOT NULL CHECK (
        channel_kind IN ('MAR','FOG','SAT','DRAIN','SEWER','TEST_RIG')
    ),
    segment_label       TEXT NOT NULL,        -- freeform, e.g. "Gila-West-Cell-03"
    install_utc         TEXT NOT NULL,
    remove_utc          TEXT,                 -- NULL when still installed
    mass_kg             REAL NOT NULL CHECK (mass_kg >= 0.0),
    surface_area_m2     REAL NOT NULL CHECK (surface_area_m2 >= 0.0),
    duty_cycle_hint     REAL NOT NULL CHECK (duty_cycle_hint BETWEEN 0.0 AND 1.0),
    notes               TEXT
);

CREATE INDEX IF NOT EXISTS idx_cybo_channel_material_node
    ON cybo_channel_material (node_id, channel_kind, install_utc);

CREATE INDEX IF NOT EXISTS idx_cybo_channel_material_batch
    ON cybo_channel_material (batch_id);


-------------------------------------------------------------------------------
-- 3. Cyboquatic biodegradable performance ledger (non‑actuating diagnostics)
-------------------------------------------------------------------------------

-- Table: cybo_channel_perf_ledger
-- Purpose:
-- - Time‑series of eco‑performance metrics for biodegradable channels.
-- - Tied to existing Lyapunov/KER residuals via vt_before/vt_after.
-- - Non‑actuating: all decisions are evidence of what occurred, not triggers.
--
-- Metrics:
-- - energy_j_in, energy_j_out: per‑window channel energy balance
-- - fog_captured_kg, organics_captured_kg: FOG + solids removal
-- - pollutant_reduction_frac: normalized removal of PFAS, BOD, E. coli, etc.
-- - rcarbon, rmaterials, rwater, rbio: KER‑aligned risk coordinates
-- - vt_before, vt_after: residual snapshots for this channel + node window
-- - decision: ACCEPT / REJECT / REROUTE, diagnostic only

CREATE TABLE IF NOT EXISTS cybo_channel_perf_ledger (
    ledger_id               INTEGER PRIMARY KEY AUTOINCREMENT,
    deploy_id               INTEGER NOT NULL
                                REFERENCES cybo_channel_material(deploy_id)
                                ON DELETE CASCADE,
    node_id                 TEXT NOT NULL,
    window_start_utc        TEXT NOT NULL,
    window_end_utc          TEXT NOT NULL,
    energy_j_in             REAL NOT NULL CHECK (energy_j_in >= 0.0),
    energy_j_out            REAL NOT NULL CHECK (energy_j_out >= 0.0),
    fog_captured_kg         REAL NOT NULL CHECK (fog_captured_kg >= 0.0),
    organics_captured_kg    REAL NOT NULL CHECK (organics_captured_kg >= 0.0),
    pollutant_reduction_frac REAL NOT NULL
                              CHECK (pollutant_reduction_frac BETWEEN 0.0 AND 1.0),
    rcarbon                 REAL NOT NULL CHECK (rcarbon BETWEEN 0.0 AND 1.0),
    rmaterials              REAL NOT NULL CHECK (rmaterials BETWEEN 0.0 AND 1.0),
    rwater                  REAL NOT NULL CHECK (rwater BETWEEN 0.0 AND 1.0),
    rbio                    REAL NOT NULL CHECK (rbio BETWEEN 0.0 AND 1.0),
    vt_before               REAL NOT NULL,
    vt_after                REAL NOT NULL CHECK (vt_after <= vt_before + 1e-6),
    decision                TEXT NOT NULL CHECK (
        decision IN ('ACCEPT','REJECT','REROUTE')
    ),
    created_utc             TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_cybo_channel_perf_node_time
    ON cybo_channel_perf_ledger (node_id, window_start_utc, window_end_utc);

CREATE INDEX IF NOT EXISTS idx_cybo_channel_perf_deploy
    ON cybo_channel_perf_ledger (deploy_id, decision);


-------------------------------------------------------------------------------
-- 4. Views: biodegradable eco‑performance and KER‑grade summaries
-------------------------------------------------------------------------------

-- View: v_cybo_channel_perf_window
-- Purpose:
-- - Aggregate perf ledger for a given node/channel over any time window.
-- - Read‑only diagnostic surface for dashboards and AI chat.
-- - Energy efficiency, capture yield, and residual trend.

CREATE VIEW IF NOT EXISTS v_cybo_channel_perf_window AS
SELECT
    c.node_id                              AS node_id,
    c.channel_kind                         AS channel_kind,
    MIN(l.window_start_utc)                AS window_start_utc,
    MAX(l.window_end_utc)                  AS window_end_utc,
    SUM(l.energy_j_in)                     AS energy_j_in_total,
    SUM(l.energy_j_out)                    AS energy_j_out_total,
    SUM(l.fog_captured_kg)                 AS fog_captured_kg_total,
    SUM(l.organics_captured_kg)            AS organics_captured_kg_total,
    AVG(l.pollutant_reduction_frac)        AS pollutant_reduction_avg,
    AVG(l.rcarbon)                         AS rcarbon_mean,
    AVG(l.rmaterials)                      AS rmaterials_mean,
    AVG(l.rwater)                          AS rwater_mean,
    AVG(l.rbio)                            AS rbio_mean,
    AVG(l.vt_before)                       AS vt_before_mean,
    AVG(l.vt_after)                        AS vt_after_mean,
    AVG(l.vt_before - l.vt_after)          AS vt_gain_mean,
    SUM(CASE WHEN l.decision = 'ACCEPT' THEN 1 ELSE 0 END)
        AS n_accept,
    SUM(CASE WHEN l.decision = 'REJECT' THEN 1 ELSE 0 END)
        AS n_reject,
    SUM(CASE WHEN l.decision = 'REROUTE' THEN 1 ELSE 0 END)
        AS n_reroute
FROM cybo_channel_material c
JOIN cybo_channel_perf_ledger l
  ON l.deploy_id = c.deploy_id
GROUP BY
    c.node_id,
    c.channel_kind;


-- View: v_cybo_material_ker_profile
-- Purpose:
-- - Project batches into a compact KER‑style eco‑profile.
-- - Allows EcoNet spine and AI agents to rank biodegradable recipes.
-- - Pure read‑only function of cybo_material_batch; no actuation.

CREATE VIEW IF NOT EXISTS v_cybo_material_ker_profile AS
SELECT
    batch_id,
    repo_name,
    material_family,
    region_code,
    manufacture_utc,
    -- Simple scalar summaries; can be refined as needed.
    (1.0 - rmass_loss)                         AS k_structural_integrity,
    (1.0 - rresidue)                           AS k_low_residue,
    (1.0 - rfragment)                          AS k_low_fragment,
    (1.0 - rcarbon_embodied)                   AS e_carbon_benefit,
    (1.0 - rleachate)                          AS e_leachate_safety,
    (1.0 - rtox_aquatic)                       AS e_tox_safety,
    (1.0 - rbiodiv)                            AS r_biodiv_headroom,
    lane_default,
    ker_k_target,
    ker_e_target,
    ker_r_ceiling,
    non_actuating_only,
    did_owner,
    signing_did
FROM cybo_material_batch;


-------------------------------------------------------------------------------
-- 5. Seed rows: Phoenix biodegradable tray + FOG baffle batches
--    All values are example evidence; replace with measured data when ready.
-------------------------------------------------------------------------------

INSERT OR IGNORE INTO cybo_material_batch (
    batch_id,
    repo_name,
    material_family,
    description,
    manufacture_utc,
    region_code,
    process_route,
    rmass_loss,
    rresidue,
    rfragment,
    rcarbon_embodied,
    rleachate,
    rtox_aquatic,
    rbiodiv,
    lane_default,
    ker_k_target,
    ker_e_target,
    ker_r_ceiling,
    non_actuating_only,
    did_owner,
    signing_did,
    evidence_hex
) VALUES
(
    'BioSub-2026-03A',
    'ecorestorationshard',
    'tray',
    'Phoenix pilot biodegradable tray: cellulose + plant oil blend, SAT-safe.',
    '2026-03-10T00:00:00Z',
    'Phoenix-AZ',
    'thermoformed-cellulose-plant-oil-2026v1',
    0.18,   -- rmass_loss (good decomposition)
    0.12,   -- rresidue (low solid residue)
    0.15,   -- rfragment (low micro‑fragmentation)
    0.22,   -- rcarbon_embodied
    0.16,   -- rleachate
    0.10,   -- rtox_aquatic
    0.14,   -- rbiodiv
    'RESEARCH',
    0.94,
    0.90,
    0.12,
    1,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    NULL,
    NULL
),
(
    'BioSub-2026-03B',
    'ecorestorationshard',
    'baffle',
    'Phoenix sewer FOG baffle: lighter‑mass substrate for desiccator channels.',
    '2026-03-11T00:00:00Z',
    'Phoenix-AZ',
    'foam-cellulose-mineral-fill-2026v1',
    0.20,
    0.18,
    0.19,
    0.24,
    0.20,
    0.12,
    0.18,
    'RESEARCH',
    0.94,
    0.90,
    0.13,
    1,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    NULL,
    NULL
);


INSERT OR IGNORE INTO cybo_channel_material (
    batch_id,
    node_id,
    channel_kind,
    segment_label,
    install_utc,
    remove_utc,
    mass_kg,
    surface_area_m2,
    duty_cycle_hint,
    notes
) VALUES
(
    'BioSub-2026-03A',
    'PHX-CYBOQ-MAR-001',
    'MAR',
    'SAT-Cell-01',
    '2026-03-15T08:00:00Z',
    NULL,
    120.0,
    45.0,
    0.85,
    'Initial MAR tray installation for Phoenix SAT pilot cell 01.'
),
(
    'BioSub-2026-03B',
    'PHX-CYBOQ-CHANNEL-FOG-01',
    'FOG',
    'Sewer-FOG-Globe-01',
    '2026-03-16T06:00:00Z',
    NULL,
    40.0,
    28.0,
    0.72,
    'FOG desiccator baffles in Phoenix sewer pilot, globe 01.'
);


INSERT INTO cybo_channel_perf_ledger (
    deploy_id,
    node_id,
    window_start_utc,
    window_end_utc,
    energy_j_in,
    energy_j_out,
    fog_captured_kg,
    organics_captured_kg,
    pollutant_reduction_frac,
    rcarbon,
    rmaterials,
    rwater,
    rbio,
    vt_before,
    vt_after,
    decision,
    created_utc
)
SELECT
    cm.deploy_id,
    cm.node_id,
    '2026-04-01T00:00:00Z',
    '2026-04-30T23:59:59Z',
    8.5e8,          -- energy_j_in
    7.9e8,          -- energy_j_out
    1.8e3,          -- fog_captured_kg
    3.2e3,          -- organics_captured_kg
    0.78,           -- pollutant_reduction_frac
    0.21,           -- rcarbon
    0.19,           -- rmaterials
    0.17,           -- rwater
    0.16,           -- rbio
    0.40,           -- vt_before
    0.386,          -- vt_after (non‑increasing)
    'ACCEPT',
    '2026-05-01T00:05:00Z'
FROM cybo_channel_material cm
WHERE cm.batch_id = 'BioSub-2026-03A'
LIMIT 1;


INSERT INTO cybo_channel_perf_ledger (
    deploy_id,
    node_id,
    window_start_utc,
    window_end_utc,
    energy_j_in,
    energy_j_out,
    fog_captured_kg,
    organics_captured_kg,
    pollutant_reduction_frac,
    rcarbon,
    rmaterials,
    rwater,
    rbio,
    vt_before,
    vt_after,
    decision,
    created_utc
)
SELECT
    cm.deploy_id,
    cm.node_id,
    '2026-04-01T00:00:00Z',
    '2026-04-30T23:59:59Z',
    4.1e8,
    3.9e8,
    1.1e3,
    2.4e3,
    0.71,
    0.23,
    0.21,
    0.19,
    0.18,
    0.39,
    0.383,
    'ACCEPT',
    '2026-05-01T00:06:00Z'
FROM cybo_channel_material cm
WHERE cm.batch_id = 'BioSub-2026-03B'
LIMIT 1;
