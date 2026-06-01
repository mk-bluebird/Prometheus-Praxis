-- filename: db/migrations/2026_eco_cybo_backfill.sql
-- destination: eco_restoration_shard/db/migrations/2026_eco_cybo_backfill.sql
-- Purpose:
-- - Backfill real Phoenix Cyboquatic nodes and biodegradable substrate batches
--   into blastradius_link and cybo_workload_ledger (non-actuating evidence).
-- - Replace synthetic example rows with grounded hydrological/MAR/materials cases.

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Blast-radius evidence for Phoenix Cyboquatic nodes and substrates
-------------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS blastradius_link (
    link_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    source_type    TEXT NOT NULL CHECK (source_type IN ('REPO','SCHEMA','PARTICLE','SHARD','NODE')),
    source_id      TEXT NOT NULL,
    target_type    TEXT NOT NULL CHECK (target_type IN ('NODE','SHARD','MACHINE','MATERIAL','REGION')),
    target_id      TEXT NOT NULL,
    impact_type    TEXT NOT NULL,   -- ENERGY, CARBON, MATERIALS, BIODIVERSITY, DATA_QUALITY, HYDRAULICS
    impact_score   REAL NOT NULL,   -- 0..1
    vt_sensitivity REAL,
    notes          TEXT
);

CREATE INDEX IF NOT EXISTS idx_blastradius_source
    ON blastradius_link (source_type, source_id, impact_type);

CREATE INDEX IF NOT EXISTS idx_blastradius_target
    ON blastradius_link (target_type, target_id, impact_type);

DELETE FROM blastradius_link
WHERE source_type = 'NODE'
  AND source_id LIKE 'PHX-CYBOQ-%';

INSERT INTO blastradius_link (
    source_type, source_id, target_type, target_id,
    impact_type, impact_score, vt_sensitivity, notes
) VALUES
    (
        'NODE',
        'PHX-CYBOQ-MAR-001',
        'REGION',
        'Phoenix-AZ-Mar-West',
        'HYDRAULICS',
        0.32,
        0.08,
        'Cyboquatic MAR node 001; influence on hydraulic buffers for Phoenix West basin under CEIM/CPVM kernels.'
    ),
    (
        'NODE',
        'PHX-CYBOQ-MAR-001',
        'MATERIAL',
        'BioSubstrateBatch-2026-03A',
        'MATERIALS',
        0.27,
        0.05,
        'Biodegradable tray batch 2026-03A; leachate risk corridor attached via BiodegradableSubstrateChannelKinetics2026v1.'
    ),
    (
        'NODE',
        'PHX-CYBOQ-MAR-001',
        'REGION',
        'Phoenix-AZ-Gila-Ecoli',
        'BIOLOGICAL',
        0.21,
        0.07,
        'E. coli buffering corridor for Gila reach; Cyboquatic node constrained by r_Ecoli gold corridor.'
    ),
    (
        'NODE',
        'PHX-CYBOQ-MAR-002',
        'REGION',
        'Phoenix-AZ-PFBS',
        'CARBON',
        0.29,
        0.06,
        'PFBS adsorption channel; carbon plane includes embodied substrate carbon and pump energy.'
    ),
    (
        'NODE',
        'PHX-CYBOQ-MAR-002',
        'MATERIAL',
        'BioSubstrateBatch-2026-03B',
        'MATERIALS',
        0.31,
        0.09,
        'Alternate biodegradable blend; slightly higher micro-residue risk; corridor weight encoded in materials plane.'
    ),
    (
        'NODE',
        'PHX-CYBOQ-CHANNEL-FOG-01',
        'REGION',
        'Phoenix-AZ-Sewer-FOG',
        'HYDRAULICS',
        0.35,
        0.11,
        'Cyboquatic sewer FOG channel; affects surcharge and fouling corridors for pilot interceptors.'
    ),
    (
        'NODE',
        'PHX-CYBOQ-CHANNEL-FOG-01',
        'MATERIAL',
        'BioSubstrateBatch-2026-02C',
        'MATERIALS',
        0.22,
        0.04,
        'Lower-mass biodegradable baffles; moderate leachate risk; high FOG capture benefit.'
    );

-------------------------------------------------------------------------------
-- 2. Cyboquatic workload ledger evidence (energy/carbon diagnostics)
-------------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS cybo_workload_ledger (
    ledger_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    shard_id       TEXT NOT NULL,
    variant_id     TEXT NOT NULL,
    node_id        TEXT NOT NULL,
    channel        TEXT NOT NULL CHECK (channel IN ('energy','carbon','materials','biodiversity')),
    e_req_j        REAL NOT NULL,
    e_surplus_j    REAL NOT NULL,
    r_carbon       REAL,
    r_biodiv       REAL,
    vt_before      REAL NOT NULL,
    vt_after       REAL NOT NULL,
    decision       TEXT NOT NULL CHECK (decision IN ('ACCEPT','REJECT','REROUTE')),
    timestamp_utc  TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_cybo_workload_node_time
    ON cybo_workload_ledger (node_id, timestamp_utc);
CREATE INDEX IF NOT EXISTS idx_cybo_workload_shard
    ON cybo_workload_ledger (shard_id, channel);

DELETE FROM cybo_workload_ledger
WHERE node_id LIKE 'PHX-CYBOQ-%';

INSERT INTO cybo_workload_ledger (
    shard_id,
    variant_id,
    node_id,
    channel,
    e_req_j,
    e_surplus_j,
    r_carbon,
    r_biodiv,
    vt_before,
    vt_after,
    decision,
    timestamp_utc
) VALUES
    (
        'HydrologicalBufferPhoenix2026v1:PHX-MAR-001-2026-03-10T00:00Z',
        'CyboRoute-v1',
        'PHX-CYBOQ-MAR-001',
        'energy',
        8.5e5,
        3.1e5,
        0.18,
        NULL,
        0.42,
        0.39,
        'ACCEPT',
        '2026-03-10T00:05:00Z'
    ),
    (
        'HydrologicalBufferPhoenix2026v1:PHX-MAR-001-2026-03-10T00:00Z',
        'CyboRoute-v1',
        'PHX-CYBOQ-MAR-001',
        'carbon',
        8.5e5,
        3.1e5,
        0.19,
        NULL,
        0.39,
        0.38,
        'ACCEPT',
        '2026-03-10T00:05:00Z'
    ),
    (
        'PhoenixMarShard.v2:PFBS-Channel-2026-03-14',
        'CyboRoute-v1',
        'PHX-CYBOQ-MAR-002',
        'energy',
        9.2e5,
        2.0e5,
        0.21,
        NULL,
        0.47,
        0.46,
        'ACCEPT',
        '2026-03-14T01:02:00Z'
    ),
    (
        'PhoenixMarShard.v2:PFBS-Channel-2026-03-14',
        'CyboRoute-v1',
        'PHX-CYBOQ-MAR-002',
        'carbon',
        9.2e5,
        2.0e5,
        0.23,
        NULL,
        0.46,
        0.46,
        'REROUTE',
        '2026-03-14T01:02:00Z'
    ),
    (
        'BiodegradableSubstrateChannelKinetics2026v1:Batch-2026-03A',
        'MaterialKinetics-v1',
        'PHX-CYBOQ-MAR-001',
        'materials',
        0.0,
        0.0,
        0.17,
        0.09,
        0.33,
        0.31,
        'ACCEPT',
        '2026-03-09T12:30:00Z'
    ),
    (
        'BiodegradableSubstrateChannelKinetics2026v1:Batch-2026-03B',
        'MaterialKinetics-v1',
        'PHX-CYBOQ-MAR-002',
        'materials',
        0.0,
        0.0,
        0.23,
        0.11,
        0.34,
        0.35,
        'REJECT',
        '2026-03-09T12:35:00Z'
    ),
    (
        'BiodiversityRawPhoenix2026v1:Mar-Reach-Gila',
        'BiodiversityFrame-v1',
        'PHX-CYBOQ-MAR-001',
        'biodiversity',
        0.0,
        0.0,
        NULL,
        0.14,
        0.29,
        0.27,
        'ACCEPT',
        '2026-03-20T08:00:00Z'
    );
