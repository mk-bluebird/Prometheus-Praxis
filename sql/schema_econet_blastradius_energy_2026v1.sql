-- filename: schema_econet_blastradius_energy_2026v1.sql
-- destination: mk-bluebird/eco_restoration_shard/sql/schema_econet_blastradius_energy_2026v1.sql
-- role: central, non‑actuating SQLite spine for EcoNet/Cyboquatic blastradius + energy/carbon always‑improve logic

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Core lookup tables (align with existing EcoNet spine)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS repo (
    repo_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    reponame       TEXT NOT NULL UNIQUE,
    github_slug    TEXT NOT NULL,
    roleband       TEXT NOT NULL,   -- SPINE,ENGINE,MATERIAL,GOV,APP,RESEARCH
    visibility     TEXT NOT NULL,   -- Public,Private
    language_primary TEXT NOT NULL, -- Rust,C++,Lua,Kotlin,Other
    description    TEXT,
    ecosafety_binding TEXT NOT NULL, -- e.g. cyboquatic-ecosafety-core-2026v1
    shard_protocol TEXT NOT NULL,    -- e.g. ALN-RFC4180-EcoNetSchemaShard-2026v1
    lane_default   TEXT NOT NULL,    -- RESEARCH,EXPPROD,PROD
    ker_target_k   REAL NOT NULL,
    ker_target_e   REAL NOT NULL,
    ker_target_r   REAL NOT NULL,
    nonactuating_only INTEGER NOT NULL CHECK (nonactuating_only IN (0,1))
);

CREATE TABLE IF NOT EXISTS shardinstance (
    shard_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_id        INTEGER NOT NULL REFERENCES repo(repo_id) ON DELETE CASCADE,
    plane_contract_id TEXT NOT NULL,
    lane           TEXT NOT NULL,
    ker_k          REAL NOT NULL,
    ker_e          REAL NOT NULL,
    ker_r          REAL NOT NULL,
    vt_max         REAL NOT NULL,
    ker_deployable INTEGER NOT NULL CHECK (ker_deployable IN (0,1)),
    window_start_utc TEXT NOT NULL,
    window_end_utc   TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_shard_repo_lane
    ON shardinstance (repo_id, lane, ker_deployable);

----------------------------------------------------------------------
-- 2. Blast radius index (pure impact surfaces, non‑actuating)
----------------------------------------------------------------------

-- Each row describes how a source (repo/schema/particle/shard/file)
-- geometrically or logically touches a target (node/machine/material/region).
-- No actuators, only diagnostics for Cyboquatic/EcoNet planning.

CREATE TABLE IF NOT EXISTS blastradius_link (
    link_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    source_type    TEXT NOT NULL CHECK (source_type IN ('REPO','SCHEMA','PARTICLE','SHARD','FILE')),
    source_id      TEXT NOT NULL,         -- FK encoded as stable string, e.g. repo name, shard hex id
    target_type    TEXT NOT NULL CHECK (target_type IN ('NODE','SHARD','MACHINE','MATERIAL','REGION')),
    target_id      TEXT NOT NULL,         -- e.g. node DID, machine code, watershed id
    impact_type    TEXT NOT NULL CHECK (
                      impact_type IN (
                        'HYDRAULIC','ENERGY','CARBON','BIODIVERSITY','MATERIAL','DATAQUALITY','GOVERNANCE'
                      )
                    ),
    impact_score   REAL NOT NULL,         -- 0..1 normalized fraction of corridor width touched
    vt_sensitivity REAL,                  -- approximate |ΔV| per unit change in this source (dimensionless)
    notes          TEXT
);

CREATE INDEX IF NOT EXISTS idx_blastradius_source
    ON blastradius_link (source_type, source_id, impact_type);

CREATE INDEX IF NOT EXISTS idx_blastradius_target
    ON blastradius_link (target_type, target_id, impact_type);

----------------------------------------------------------------------
-- 3. Cyboquatic workload energy/carbon ledger (readonly from control)
----------------------------------------------------------------------

-- Captures accepted / rejected workloads for Cyboquatic machines.
-- Used to learn cheaper, carbon‑negative, eco‑restorative trajectories.
-- Controllers remain fenced by Rust/ALN ecosafety kernels.

CREATE TABLE IF NOT EXISTS workload_ledger (
    ledger_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    shard_id       INTEGER NOT NULL REFERENCES shardinstance(shard_id) ON DELETE CASCADE,
    variant_id     TEXT NOT NULL,   -- ALN variant id or machinery configuration id
    node_id        TEXT NOT NULL,   -- Cyboquatic node DID / equipment id
    channel        TEXT NOT NULL CHECK (channel IN ('energy','carbon','materials','biodiversity')),
    e_req_j        REAL NOT NULL,   -- requested energy (J)
    e_surplus_j    REAL NOT NULL,   -- local surplus at dispatch (J)
    r_carbon       REAL,            -- normalized carbon risk coordinate
    r_biodiv       REAL,            -- normalized biodiversity risk coordinate
    vt_before      REAL NOT NULL,   -- residual before dispatch
    vt_after       REAL NOT NULL,   -- residual after dispatch (simulated or logged)
    decision       TEXT NOT NULL CHECK (decision IN ('ACCEPT','REJECT','REROUTE')),
    timestamp_utc  TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_workload_node_time
    ON workload_ledger (node_id, timestamp_utc);

CREATE INDEX IF NOT EXISTS idx_workload_shard
    ON workload_ledger (shard_id, channel);

----------------------------------------------------------------------
-- 4. Views for always‑improve loops (non‑actuating analytics)
----------------------------------------------------------------------

-- 4.1 Per‑shard Lyapunov step check (cheap surrogate)
CREATE VIEW IF NOT EXISTS v_shard_safestep AS
SELECT
    w.ledger_id,
    w.shard_id,
    w.node_id,
    w.channel,
    w.vt_before,
    w.vt_after,
    (w.vt_after - w.vt_before) AS dv,
    CASE WHEN (w.vt_after - w.vt_before) <= 0.0 THEN 1 ELSE 0 END AS safestep_ok
FROM workload_ledger AS w;

-- 4.2 Energy cost and carbon trend per node (for Cyboquatic routing research)
CREATE VIEW IF NOT EXISTS v_node_energy_carbon AS
SELECT
    node_id,
    COUNT(*)                              AS n_events,
    SUM(CASE WHEN decision = 'ACCEPT' THEN e_req_j ELSE 0 END) AS e_req_accept_j,
    SUM(CASE WHEN decision = 'ACCEPT' THEN e_surplus_j ELSE 0 END) AS e_surplus_accept_j,
    AVG(r_carbon)                         AS r_carbon_avg,
    AVG(r_biodiv)                         AS r_biodiv_avg,
    AVG(vt_after - vt_before)            AS dv_avg
FROM workload_ledger
GROUP BY node_id;

-- 4.3 Blast radius summary per source (what gets touched if we change this)
CREATE VIEW IF NOT EXISTS v_blastradius_source_summary AS
SELECT
    source_type,
    source_id,
    impact_type,
    COUNT(*)                  AS n_targets,
    AVG(impact_score)         AS impact_score_avg,
    MAX(impact_score)         AS impact_score_peak,
    AVG(COALESCE(vt_sensitivity, 0.0)) AS vt_sensitivity_avg
FROM blastradius_link
GROUP BY source_type, source_id, impact_type;

-- 4.4 Candidate eco‑restorative, low‑energy machines/materials
-- “Good” if: high average impact_score on CARBON or BIODIVERSITY,
-- low vt_sensitivity, and non‑positive dv in ledger.
CREATE VIEW IF NOT EXISTS v_candidate_ecorestorative AS
SELECT
    b.source_type,
    b.source_id,
    MAX(CASE WHEN b.impact_type = 'CARBON' THEN b.impact_score ELSE 0 END)      AS impact_carbon,
    MAX(CASE WHEN b.impact_type = 'BIODIVERSITY' THEN b.impact_score ELSE 0 END) AS impact_biodiv,
    AVG(COALESCE(b.vt_sensitivity, 0.0))                                        AS vt_sensitivity_avg,
    AVG(COALESCE(s.dv,0.0))                                                    AS dv_avg
FROM blastradius_link AS b
LEFT JOIN v_shard_safestep AS s
    ON s.shard_id = CASE WHEN b.source_type = 'SHARD' THEN CAST(b.source_id AS INTEGER) ELSE NULL END
GROUP BY b.source_type, b.source_id
HAVING
    (impact_carbon >= 0.5 OR impact_biodiv >= 0.5)
    AND vt_sensitivity_avg <= 0.1
    AND dv_avg <= 0.0;

----------------------------------------------------------------------
-- 5. Repo‑local master index table (for cross‑repo constellation logic)
----------------------------------------------------------------------

-- This same schema can be copied as .econet_repo_index.sql into each repo,
-- then ingested into the central spine to give agents a non‑actuating,
-- machine‑readable map of EcoNet/Cyboquatic roles.

CREATE TABLE IF NOT EXISTS econet_repoindex (
    reponame          TEXT PRIMARY KEY,
    github_slug       TEXT NOT NULL,
    roleband          TEXT NOT NULL,
    visibility        TEXT NOT NULL,
    language_primary  TEXT NOT NULL,
    description       TEXT,
    ecosafety_binding TEXT NOT NULL,
    shard_protocol    TEXT NOT NULL,
    lane_default      TEXT NOT NULL,
    ker_target_k      REAL NOT NULL,
    ker_target_e      REAL NOT NULL,
    ker_target_r      REAL NOT NULL,
    nonactuating_only INTEGER NOT NULL CHECK (nonactuating_only IN (0,1))
);

CREATE TABLE IF NOT EXISTS econet_layer (
    layer_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    reponame   TEXT NOT NULL REFERENCES econet_repoindex(reponame) ON DELETE CASCADE,
    layer_name TEXT NOT NULL,
    layer_tier TEXT NOT NULL, -- GRAMMAR,KERNEL,EDGESCRIPT,UI,GOVERNANCE,MATERIAL,OTHER
    languages  TEXT NOT NULL, -- e.g. "Rust", "Rust,Lua", "C++"
    description TEXT,
    contracts   TEXT          -- human‑readable invariants (non‑actuating, Vt only, etc.)
);

-- Example seed row for mk-bluebird/eco_restoration_shard itself
INSERT OR IGNORE INTO econet_repoindex (
    reponame,
    github_slug,
    roleband,
    visibility,
    language_primary,
    description,
    ecosafety_binding,
    shard_protocol,
    lane_default,
    ker_target_k,
    ker_target_e,
    ker_target_r,
    nonactuating_only
) VALUES (
    'eco_restoration_shard',
    'mk-bluebird/eco_restoration_shard',
    'SPINE',
    'Public',
    'Rust',
    'Ecological restoration research spine; Cyboquatic materials, KER math, and EcoNet constellation indexing.',
    'cyboquatic-ecosafety-core-2026v1',
    'ALN-RFC4180-EcoNetSchemaShard-2026v1',
    'RESEARCH',
    0.94,
    0.91,
    0.13,
    1
);
