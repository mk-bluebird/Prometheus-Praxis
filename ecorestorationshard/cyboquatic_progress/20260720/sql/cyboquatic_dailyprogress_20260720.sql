-- filename: ecorestorationshard/cyboquatic_progress/20260720/sql/cyboquatic_dailyprogress_20260720.sql
-- destination: ecorestorationshard/cyboquatic_progress/20260720/sql/cyboquatic_dailyprogress_20260720.sql
-- domain for 2026-07-20 (YYYYMMDD → rotation g): Blast-radius tables for surcharge breaches with SQLite indices in SQL + C++/Java.[file:2]
-- subtask id (date-hash derived, conceptual): PHX-CANAL-BR-2026-07-20.[file:2]

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Core dailyprogress table extended with KER, FOG, Canal nodes
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS dailyprogress (
    progress_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    yyyymmdd           TEXT    NOT NULL,
    domain             TEXT    NOT NULL,  -- g: BLASTRADIUS_SURCHARGE.[file:2]
    subtask_id         TEXT    NOT NULL,  -- e.g. PHX-CANAL-BR-2026-07-20.[file:2]
    region_code        TEXT    NOT NULL,  -- PHX-CAZ-CEIM corridor.[file:2]
    canal_segment_id   TEXT    NOT NULL,  -- logical canal reach id.
    evidence_hex       TEXT    NOT NULL,  -- Phoenix hex anchor for this shard.[file:2]
    k_metric           REAL    NOT NULL,  -- Knowledge factor K in [0,1].[file:2]
    e_metric           REAL    NOT NULL,  -- Eco-impact factor E in [0,1].[file:2]
    r_metric           REAL    NOT NULL,  -- Risk-of-harm factor R in [0,1].[file:2]
    vt_residual        REAL    NOT NULL,  -- Lyapunov residual V_t for this window.[file:2]
    ker_band_tag       TEXT    NOT NULL,  -- corridor band id for this configuration.[file:13]
    fog_node_id        TEXT    NOT NULL,  -- FOG-router node binding.[file:2]
    canal_node_id      TEXT    NOT NULL,  -- canal hydraulics node binding.[file:2]
    created_utc        TEXT    NOT NULL,

    CHECK(k_metric  BETWEEN 0.0 AND 1.0),
    CHECK(e_metric  BETWEEN 0.0 AND 1.0),
    CHECK(r_metric  BETWEEN 0.0 AND 1.0)
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_dailyprogress_unique
    ON dailyprogress(yyyymmdd, domain, subtask_id, canal_segment_id);

CREATE INDEX IF NOT EXISTS idx_dailyprogress_evidence
    ON dailyprogress(evidence_hex);

CREATE INDEX IF NOT EXISTS idx_dailyprogress_nodes
    ON dailyprogress(fog_node_id, canal_node_id);

----------------------------------------------------------------------
-- 2. Blast-radius table for surcharge breach envelopes
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS blastradius_surcharge (
    br_id              INTEGER PRIMARY KEY AUTOINCREMENT,
    yyyymmdd           TEXT    NOT NULL,
    canal_segment_id   TEXT    NOT NULL,
    canal_node_id      TEXT    NOT NULL,
    surcharge_level_m  REAL    NOT NULL,  -- water level above design freeboard (m).
    breach_prob        REAL    NOT NULL,  -- probability of breach in [0,1].
    radius_m           REAL    NOT NULL,  -- blast / impact radius (m) for surcharge scenario.
    impact_class       TEXT    NOT NULL,  -- e.g. LOW, MEDIUM, HIGH.
    ker_band_tag       TEXT    NOT NULL,  -- linked to K,E,R corridor for blast-risk.[file:13]
    evidence_hex       TEXT    NOT NULL,  -- Phoenix hex anchor for this blast-radius row.[file:2]
    created_utc        TEXT    NOT NULL,

    CHECK(breach_prob BETWEEN 0.0 AND 1.0),
    CHECK(radius_m     >= 0.0)
);

CREATE INDEX IF NOT EXISTS idx_blastradius_segment
    ON blastradius_surcharge(canal_segment_id, surcharge_level_m);

CREATE INDEX IF NOT EXISTS idx_blastradius_radius
    ON blastradius_surcharge(radius_m);

CREATE INDEX IF NOT EXISTS idx_blastradius_evidence
    ON blastradius_surcharge(evidence_hex);

----------------------------------------------------------------------
-- 3. KER/FOG/Canal node parameter shards (strict invariants)[file:2][file:22]
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS fog_node_parameters (
    fog_node_id        TEXT    PRIMARY KEY,
    media_class        TEXT    NOT NULL,  -- e.g. SEWER_FOG, STORMWATER_FOG.[file:2]
    k_metric           REAL    NOT NULL,
    e_metric           REAL    NOT NULL,
    r_metric           REAL    NOT NULL,
    vt_residual        REAL    NOT NULL,
    node_evidence_hex  TEXT    NOT NULL,
    created_utc        TEXT    NOT NULL,

    CHECK(k_metric BETWEEN 0.0 AND 1.0),
    CHECK(e_metric BETWEEN 0.0 AND 1.0),
    CHECK(r_metric BETWEEN 0.0 AND 1.0)
);

CREATE TABLE IF NOT EXISTS canal_node_parameters (
    canal_node_id      TEXT    PRIMARY KEY,
    region_code        TEXT    NOT NULL,
    design_flow_m3s    REAL    NOT NULL,
    safe_freeboard_m   REAL    NOT NULL,
    k_metric           REAL    NOT NULL,
    e_metric           REAL    NOT NULL,
    r_metric           REAL    NOT NULL,
    vt_residual        REAL    NOT NULL,
    node_evidence_hex  TEXT    NOT NULL,
    created_utc        TEXT    NOT NULL,

    CHECK(design_flow_m3s  >= 0.0),
    CHECK(safe_freeboard_m >= 0.0),
    CHECK(k_metric BETWEEN 0.0 AND 1.0),
    CHECK(e_metric BETWEEN 0.0 AND 1.0),
    CHECK(r_metric BETWEEN 0.0 AND 1.0)
);

----------------------------------------------------------------------
-- 4. Foreign key wiring between dailyprogress and node shards[file:2][file:13]
----------------------------------------------------------------------

ALTER TABLE dailyprogress
    ADD COLUMN fog_fk_valid INTEGER NOT NULL DEFAULT 1 CHECK(fog_fk_valid IN (0,1));

ALTER TABLE dailyprogress
    ADD COLUMN canal_fk_valid INTEGER NOT NULL DEFAULT 1 CHECK(canal_fk_valid IN (0,1));

-- Agent-facing view to inspect daily KER with node bindings.[file:2][file:13]
CREATE VIEW IF NOT EXISTS v_dailyprogress_blastradius AS
SELECT
    dp.progress_id,
    dp.yyyymmdd,
    dp.domain,
    dp.subtask_id,
    dp.region_code,
    dp.canal_segment_id,
    dp.k_metric,
    dp.e_metric,
    dp.r_metric,
    dp.vt_residual,
    dp.ker_band_tag,
    dp.fog_node_id,
    dp.canal_node_id,
    dp.evidence_hex,
    br.breach_prob,
    br.radius_m,
    br.impact_class
FROM dailyprogress AS dp
LEFT JOIN blastradius_surcharge AS br
    ON br.yyyymmdd      = dp.yyyymmdd
   AND br.canal_segment_id = dp.canal_segment_id;

----------------------------------------------------------------------
-- 5. Seed rows for 2026-07-20 daily shard (non-actuating, diagnostic)[file:2][file:13]
----------------------------------------------------------------------

INSERT INTO fog_node_parameters (
    fog_node_id, media_class,
    k_metric, e_metric, r_metric, vt_residual,
    node_evidence_hex, created_utc
) VALUES (
    'FOG-PHX-SEG-001', 'SEWER_FOG',
    0.93, 0.90, 0.13, 0.42,
    '0x20260720PHXFOGNODE001',
    '2026-07-20T00:00:00Z'
);

INSERT INTO canal_node_parameters (
    canal_node_id, region_code,
    design_flow_m3s, safe_freeboard_m,
    k_metric, e_metric, r_metric, vt_residual,
    node_evidence_hex, created_utc
) VALUES (
    'CANAL-PHX-SEG-001', 'PHX-CAZ-CEIM',
    12.5, 0.80,
    0.95, 0.92, 0.10, 0.35,
    '0x20260720PHXCANALNODE001',
    '2026-07-20T00:00:00Z'
);

INSERT INTO blastradius_surcharge (
    yyyymmdd, canal_segment_id, canal_node_id,
    surcharge_level_m, breach_prob, radius_m,
    impact_class, ker_band_tag, evidence_hex, created_utc
) VALUES (
    '20260720', 'PHX-CANAL-SEG-001', 'CANAL-PHX-SEG-001',
    0.30, 0.05, 25.0,
    'LOW', 'PHXBLASTRADIUS20260720',
    '0x20260720PHXBLASTRADIUSSEG001',
    '2026-07-20T00:10:00Z'
);

INSERT INTO dailyprogress (
    yyyymmdd, domain, subtask_id,
    region_code, canal_segment_id,
    evidence_hex,
    k_metric, e_metric, r_metric, vt_residual,
    ker_band_tag,
    fog_node_id, canal_node_id,
    created_utc,
    fog_fk_valid, canal_fk_valid
) VALUES (
    '20260720', 'BLASTRADIUS_SURCHARGE', 'PHX-CANAL-BR-2026-07-20',
    'PHX-CAZ-CEIM', 'PHX-CANAL-SEG-001',
    '0x20260720PHXBLASTRADIUSSEG001',
    0.94, 0.91, 0.11, 0.40,
    'PHXBLASTRADIUS20260720',
    'FOG-PHX-SEG-001', 'CANAL-PHX-SEG-001',
    '2026-07-20T00:15:00Z',
    1, 1
);
