-- filename: db/db_cyboquatic_spine_index.sql
-- destination: eco_restoration_shard/db/db_cyboquatic_spine_index.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

--------------------------------------------------------------------
-- 1. Cyboquatic evidence spine: batches, deployments, performance
--------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_material_batch (
    batch_id              TEXT PRIMARY KEY,             -- e.g. "BioSub-2026-03A"
    description           TEXT NOT NULL,
    manufacture_utc       TEXT NOT NULL,                -- ISO-8601

    -- Inherent eco-risk coordinates for this substrate, 0–1 (normalized)
    rmass_loss            REAL NOT NULL,                -- mass loss profile
    rresidue              REAL NOT NULL,                -- residual fragments
    rfragment             REAL NOT NULL,                -- micro-fragmentation risk
    rcarbon_embodied      REAL NOT NULL,                -- embodied carbon risk
    rleachate             REAL NOT NULL,                -- leachate toxicity
    rtox_aquatic          REAL NOT NULL,                -- aquatic toxicity
    rbiodiv               REAL NOT NULL,                -- biodiversity disturbance

    -- Governance / lane defaults and KER targets for this batch
    lane_default          TEXT NOT NULL,                -- RESEARCH/EXPPROD/PROD
    ker_k_target          REAL NOT NULL,                -- e.g. 0.90
    ker_e_target          REAL NOT NULL,                -- e.g. 0.90
    ker_r_ceiling         REAL NOT NULL,                -- e.g. 0.13

    -- Identity / governance bindings
    did_owner             TEXT NOT NULL,                -- Bostrom DID of owner
    signing_did           TEXT NOT NULL,                -- DID that signed batch spec
    evidencehex           TEXT NOT NULL,                -- hex bundle of lab / spec evidence
    spechash              TEXT NOT NULL,                -- content hash of batch spec
    created_utc           TEXT NOT NULL,
    updated_utc           TEXT NOT NULL,

    CHECK (rmass_loss      BETWEEN 0.0 AND 1.0),
    CHECK (rresidue        BETWEEN 0.0 AND 1.0),
    CHECK (rfragment       BETWEEN 0.0 AND 1.0),
    CHECK (rcarbon_embodied BETWEEN 0.0 AND 1.0),
    CHECK (rleachate       BETWEEN 0.0 AND 1.0),
    CHECK (rtox_aquatic    BETWEEN 0.0 AND 1.0),
    CHECK (rbiodiv         BETWEEN 0.0 AND 1.0),
    CHECK (ker_k_target    BETWEEN 0.0 AND 1.0),
    CHECK (ker_e_target    BETWEEN 0.0 AND 1.0),
    CHECK (ker_r_ceiling   BETWEEN 0.0 AND 1.0)
);

CREATE TABLE IF NOT EXISTS cybo_channel_material (
    deploy_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    batch_id         TEXT NOT NULL REFERENCES cybo_material_batch(batch_id) ON DELETE RESTRICT,
    node_id          TEXT NOT NULL,                  -- Cyboquatic node identifier
    channel_kind     TEXT NOT NULL,                  -- e.g. "MAR","FOG","SAT"
    segment_label    TEXT NOT NULL,                  -- freeform physical segment label

    install_utc      TEXT NOT NULL,                  -- ISO-8601
    remove_utc       TEXT,                           -- NULL while in place

    mass_kg          REAL NOT NULL,
    surface_area_m2  REAL NOT NULL,

    lane_at_install  TEXT NOT NULL,                  -- lane used at install time
    did_installer    TEXT NOT NULL,
    evidencehex      TEXT NOT NULL,
    created_utc      TEXT NOT NULL,
    updated_utc      TEXT NOT NULL,

    CHECK (mass_kg         >= 0.0),
    CHECK (surface_area_m2 >= 0.0)
);

CREATE INDEX IF NOT EXISTS idx_cybo_channel_material_node
    ON cybo_channel_material (node_id, channel_kind);

CREATE INDEX IF NOT EXISTS idx_cybo_channel_material_batch
    ON cybo_channel_material (batch_id);

CREATE TABLE IF NOT EXISTS cybo_channel_perf_ledger (
    ledger_id              INTEGER PRIMARY KEY AUTOINCREMENT,
    deploy_id              INTEGER NOT NULL
                               REFERENCES cybo_channel_material(deploy_id)
                               ON DELETE CASCADE,

    window_start_utc       TEXT NOT NULL,
    window_end_utc         TEXT NOT NULL,

    -- Energy and workload
    energy_j_in            REAL NOT NULL,
    energy_j_out           REAL NOT NULL,
    fog_captured_kg        REAL NOT NULL,
    organics_captured_kg   REAL NOT NULL,
    pollutant_reduction_frac REAL NOT NULL,   -- 0–1

    -- KER-aligned risk coordinates (observed in this window, 0–1)
    rcarbon                REAL NOT NULL,
    rmaterials             REAL NOT NULL,
    rwater                 REAL NOT NULL,
    rbio                   REAL NOT NULL,
    rcalib                 REAL NOT NULL,     -- data quality: calibration
    rsigma                 REAL NOT NULL,     -- data quality: noise
    rtopology              REAL NOT NULL,     -- adjacency / topology risk

    -- Lyapunov residual snapshots
    vt_before              REAL NOT NULL,
    vt_after               REAL NOT NULL,

    -- Governance verdict
    decision               TEXT NOT NULL,     -- ACCEPT / REJECT / REROUTE
    decision_lane          TEXT NOT NULL,     -- lane at decision time
    decision_reason        TEXT NOT NULL,

    -- Data-source trust aggregation
    dcombined              REAL NOT NULL,     -- combined data trust 0–1

    created_utc            TEXT NOT NULL,
    evidencehex            TEXT NOT NULL,

    CHECK (pollutant_reduction_frac BETWEEN 0.0 AND 1.0),
    CHECK (rcarbon    BETWEEN 0.0 AND 1.0),
    CHECK (rmaterials BETWEEN 0.0 AND 1.0),
    CHECK (rwater     BETWEEN 0.0 AND 1.0),
    CHECK (rbio       BETWEEN 0.0 AND 1.0),
    CHECK (rcalib     BETWEEN 0.0 AND 1.0),
    CHECK (rsigma     BETWEEN 0.0 AND 1.0),
    CHECK (rtopology  BETWEEN 0.0 AND 1.0),
    CHECK (dcombined  BETWEEN 0.0 AND 1.0),
    CHECK (vt_before  >= 0.0),
    CHECK (vt_after   >= 0.0),
    CHECK (decision   IN ('ACCEPT','REJECT','REROUTE'))
);

CREATE INDEX IF NOT EXISTS idx_cybo_channel_perf_ledger_deploy_time
    ON cybo_channel_perf_ledger (deploy_id, window_start_utc, window_end_utc);

--------------------------------------------------------------------
-- 2. Lyapunov enforcement view for CI/analysis (non-actuating)
--------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_cybo_perf_lyapunov_guard AS
SELECT
    l.ledger_id,
    l.deploy_id,
    l.window_start_utc,
    l.window_end_utc,
    l.vt_before,
    l.vt_after,
    (l.vt_after - l.vt_before) AS vt_delta,
    CASE
        WHEN l.vt_after <= l.vt_before THEN 1
        ELSE 0
    END AS lyapunov_ok
FROM cybo_channel_perf_ledger AS l;

--------------------------------------------------------------------
-- 3. Material KER profile view (read-only summarization)
--------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_cybo_material_ker_profile AS
SELECT
    b.batch_id,
    b.description,
    b.lane_default,
    b.rmASS_loss        AS rmass_loss,
    b.rresidue,
    b.rfragment,
    b.rcarbon_embodied,
    b.rleachate,
    b.rtox_aquatic,
    b.rbiodiv,
    b.ker_k_target,
    b.ker_e_target,
    b.ker_r_ceiling,
    b.did_owner,
    b.signing_did,
    b.spechash
FROM cybo_material_batch AS b;

--------------------------------------------------------------------
-- 4. Cyboquatic-specific index table for Ecological-Order agents
--------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_spine_cyboquatic_index (
    cybo_index_id   INTEGER PRIMARY KEY AUTOINCREMENT,

    -- File-level wiring for this cyboquatic artifact
    filename        TEXT NOT NULL,
    destination     TEXT NOT NULL,
    repo_target     TEXT NOT NULL,      -- e.g. github.com/mk-bluebird/eco_restoration_shard

    -- Logical role of the artifact in the Cyboquatic framework
    artifact_role   TEXT NOT NULL,      -- 'SQL_SCHEMA','SQL_VIEW','RUST_CDYLIB','LUA_BINDING','KOTLIN_APP'
    logical_name    TEXT NOT NULL,      -- e.g. 'CyboquaticSpine2026v1'
    version_tag     TEXT NOT NULL,      -- e.g. '2026v1'

    -- Primary ecosafety plane and main KER band this artifact touches
    primary_plane   TEXT NOT NULL,      -- e.g. 'hydraulics','materials','carbon','governance'
    lane_band       TEXT NOT NULL,      -- e.g. 'RESEARCH','EXPPROD','PROD','GOV'

    -- Binding into global definition registry (if present)
    def_logicalname TEXT,               -- mirrors definitionregistry.logicalname
    def_versiontag  TEXT,               -- mirrors definitionregistry.versiontag
    spechash        TEXT,               -- content hash of artifact spec

    -- Provenance / governance
    did_owner       TEXT NOT NULL,
    signing_did     TEXT NOT NULL,
    created_utc     TEXT NOT NULL,
    updated_utc     TEXT NOT NULL,

    UNIQUE (filename, destination, repo_target, version_tag)
);

CREATE INDEX IF NOT EXISTS idx_eco_spine_cyboquatic_role
    ON eco_spine_cyboquatic_index (artifact_role, primary_plane, lane_band);

CREATE INDEX IF NOT EXISTS idx_eco_spine_cyboquatic_def
    ON eco_spine_cyboquatic_index (def_logicalname, def_versiontag);

--------------------------------------------------------------------
-- 5. Seed rows for Phoenix baseline (non-placeholder, versioned)
--------------------------------------------------------------------

INSERT OR IGNORE INTO eco_spine_cyboquatic_index (
    filename,
    destination,
    repo_target,
    artifact_role,
    logical_name,
    version_tag,
    primary_plane,
    lane_band,
    def_logicalname,
    def_versiontag,
    spechash,
    did_owner,
    signing_did,
    created_utc,
    updated_utc
) VALUES (
    'db_cyboquatic_spine_index.sql',
    'eco_restoration_shard/db/db_cyboquatic_spine_index.sql',
    'github.com/mk-bluebird/eco_restoration_shard',
    'SQL_SCHEMA',
    'CyboquaticSpinePhoenix2026',
    '2026v1',
    'materials',
    'GOV',
    'DR.Cyboquatic.Spine.Core2026v1',
    '2026v1',
    '0x00',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    strftime('%Y-%m-%dT%H:%M:%SZ','now'),
    strftime('%Y-%m-%dT%H:%M:%SZ','now')
);
