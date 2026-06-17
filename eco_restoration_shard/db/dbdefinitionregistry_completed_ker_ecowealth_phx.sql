-- filename: dbdefinitionregistry_completed_ker_ecowealth_phx.sql
-- destination: eco_restoration_shard/db/dbdefinitionregistry_completed_ker_ecowealth_phx.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Finalize definitionscope for this DR band (KER, lanes, RoH, etc.)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS definitionscope (
    scopeid      TEXT PRIMARY KEY,
    scopename    TEXT NOT NULL,
    description  TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS definitionregistry (
    defid            INTEGER PRIMARY KEY AUTOINCREMENT,
    defname          TEXT NOT NULL UNIQUE,     -- logical name
    artifactkind     TEXT NOT NULL,            -- SQL|ALN|RUSTMODULE|WORKFLOW
    scopeid          TEXT NOT NULL,            -- FK into definitionscope
    kernelid         TEXT NOT NULL,            -- logical kernel, e.g. ecosafety.Vt.core.2026v1
    repotarget       TEXT NOT NULL,            -- e.g. eco_restoration_shard
    destinationpath  TEXT NOT NULL,            -- e.g. db/dblanelyapunovker.sql
    filename         TEXT NOT NULL,            -- e.g. dblanelyapunovker.sql
    hash             TEXT NOT NULL,            -- content hash (filled by CI tool)
    status           TEXT NOT NULL,            -- DRAFT|FROZENACTIVE|RETIRED
    active           INTEGER NOT NULL DEFAULT 0, -- 1 = usable in runtime queries
    monotoneok       INTEGER NOT NULL DEFAULT 0, -- 1 = KER / lane monotonicity verified
    evidencehex      TEXT NOT NULL,            -- ALN / proof bundle hash
    signingdid       TEXT NOT NULL,            -- e.g. bostrom18sd2...
    created_utc      TEXT NOT NULL,
    updated_utc      TEXT NOT NULL,
    FOREIGN KEY (scopeid) REFERENCES definitionscope (scopeid)
        ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_definitionregistry_scope_kernel
    ON definitionregistry (scopeid, kernelid);

CREATE INDEX IF NOT EXISTS idx_definitionregistry_status
    ON definitionregistry (status, active, monotoneok);


INSERT OR IGNORE INTO definitionscope (scopeid, scopename, description)
VALUES
    ('KERKERNEL',      'Lyapunov KER kernels',           'Core ecosafety K,E,R,Vt kernels and residuals'),
    ('LANEPOLICY',     'Lane policy contracts',          'Lane policies and admissibility predicates'),
    ('ROHMODEL',       'Risk-of-Harm kernels',           'RoH models and MT6883 healthcare kernels'),
    ('PLACEMENTPOLICY','Placement and energy policies',  'EcoNet placement and energy cost functionals'),
    ('TOPOLOGYAUDIT',  'Topology audit',                 'Topology audit kernels and rtopology projections'),
    ('QPUCATALOG',     'QPU shard catalog',              'qpushardcatalog and related QPU metadata'),
    ('ECOWEALTH',      'EcoWealth kernels and views',    'EcoUnit kernels and StewardEcoWealthStatement surfaces'),
    ('LANEHARNESS',    'Lane replay harnesses',          'Kani + Rust harnesses for lane promotion and KER replay'),
    ('VIEWGRAMMAR',    'View grammar',                   'Frozen SQL view grammar for ecosafety and ecowealth'),
    ('RESPONSIBILITY', 'ResponsibilityAxis & diversity', 'Responsibility and portfolio diversity metrics'),
    ('HEALTHCARE',     'Healthcare corridors',           'Healthcare, MT6883, and detox corridor shards'),
    ('DETOXCORRIDOR',  'Detox corridor vaults',          'Detox corridor vaults and detox guard kernels'),
    ('EVOLUTIONEPOCH', 'Evolution epochs',               'Evolution epoch and kerbandversion descriptors')
;


----------------------------------------------------------------------
-- 2. Seed rows for KER spine views (dblanelyapunovker.sql)
--    vshardresidual, vshardtopologyker, vshardker, vshardkerviolation
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry (
    defname, artifactkind, scopeid, kernelid,
    repotarget, destinationpath, filename,
    hash, status, active, monotoneok,
    evidencehex, signingdid, created_utc, updated_utc
)
VALUES
    -- Core Lyapunov residual per shard window
    ('ecosafety.vshardresidual.view.2026v1',
     'SQL',
     'KERKERNEL',
     'ecosafety.Vt.core.2026v1',
     'eco_restoration_shard',
     'db/dblanelyapunovker.sql',
     'dblanelyapunovker.sql',
     '',               -- to be filled by defreg-verify
     'FROZENACTIVE',
     1,                -- active: used by production queries
     1,                -- monotoneok: kerresidual Kani harness passes
     '',               -- to be filled from ALN evidence bundle
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     datetime('now'),
     datetime('now')
    ),

    -- Topology-augmented residual (injects rtopology)
    ('ecosafety.vshardtopologyker.view.2026v1',
     'SQL',
     'KERKERNEL',
     'ecosafety.Vt.topology.2026v1',
     'eco_restoration_shard',
     'db/dblanelyapunovker.sql',
     'dblanelyapunovker.sql',
     '',
     'FROZENACTIVE',
     1,
     1,
     '',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     datetime('now'),
     datetime('now')
    ),

    -- Full KER view with lane, deployability, vtwithtopology
    ('ecosafety.vshardker.view.2026v1',
     'SQL',
     'KERKERNEL',
     'ecosafety.KER.core.2026v1',
     'eco_restoration_shard',
     'db/dblanelyapunovker.sql',
     'dblanelyapunovker.sql',
     '',
     'FROZENACTIVE',
     1,
     1,
     '',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     datetime('now'),
     datetime('now')
    ),

    -- Lyapunov violation view
    ('ecosafety.vshardkerviolation.view.2026v1',
     'SQL',
     'KERKERNEL',
     'ecosafety.KER.violation.2026v1',
     'eco_restoration_shard',
     'db/dblanelyapunovker.sql',
     'dblanelyapunovker.sql',
     '',
     'FROZENACTIVE',
     1,
     1,
     '',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     datetime('now'),
     datetime('now')
    );


----------------------------------------------------------------------
-- 3. Lane governance surfaces (dblanegovernanceker.sql)
--    vlaneadmissibility, vrewardwindowlane
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry (
    defname, artifactkind, scopeid, kernelid,
    repotarget, destinationpath, filename,
    hash, status, active, monotoneok,
    evidencehex, signingdid, created_utc, updated_utc
)
VALUES
    ('ecosafety.vlaneadmissibility.view.2026v1',
     'SQL',
     'LANEPOLICY',
     'ecosafety.LanePolicy.core.2026v1',
     'eco_restoration_shard',
     'db/dblanegovernanceker.sql',
     'dblanegovernanceker.sql',
     '',
     'FROZENACTIVE',
     1,
     1,   -- only set to 1 after lane Kani + replay harnesses pass
     '',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     datetime('now'),
     datetime('now')
    ),

    ('ecosafety.vrewardwindowlane.view.2026v1',
     'SQL',
     'LANEPOLICY',
     'ecosafety.LaneRewardWindow.2026v1',
     'eco_restoration_shard',
     'db/dblanegovernanceker.sql',
     'dblanegovernanceker.sql',
     '',
     'FROZENACTIVE',
     1,
     1,
     '',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     datetime('now'),
     datetime('now')
    );


----------------------------------------------------------------------
-- 4. Ecowealth views and kernels
--    vecowealthview, EcoWealthKernel ALN, StewardEcoWealthStatement
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry (
    defname, artifactkind, scopeid, kernelid,
    repotarget, destinationpath, filename,
    hash, status, active, monotoneok,
    evidencehex, signingdid, created_utc, updated_utc
)
VALUES
    -- Read-only ecowealth view
    ('ecowealth.vecowealthview.view.2026v1',
     'SQL',
     'ECOWEALTH',
     'ecowealth.Kernel.2026v1',
     'eco_restoration_shard',
     'db/dbvecowealthview.sql',
     'dbvecowealthview.sql',
     '',
     'FROZENACTIVE',
     1,
     1,
     '',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     datetime('now'),
     datetime('now')
    ),

    -- EcoWealth kernel ALN particle
    ('ecowealth.ecowealthkernel.2026v1',
     'ALN',
     'ECOWEALTH',
     'ecowealth.Kernel.2026v1',
     'eco_restoration_shard',
     'aln/EcoWealthKernel2026v1.aln',
     'EcoWealthKernel2026v1.aln',
     '',
     'FROZENACTIVE',
     1,
     1,
     '',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     datetime('now'),
     datetime('now')
    ),

    -- StewardEcoWealthStatement ALN + SQL
    ('ecowealth.steward.statement.2026v1',
     'ALN',
     'ECOWEALTH',
     'ecowealth.StewardStatement.2026v1',
     'eco_restoration_shard',
     'aln/StewardEcoWealthStatement2026v1.aln',
     'StewardEcoWealthStatement2026v1.aln',
     '',
     'FROZENACTIVE',
     1,
     1,
     '',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     datetime('now'),
     datetime('now')
    );


----------------------------------------------------------------------
-- 5. Responsibility / healthcare overlays
--    vshardresponsibilityker, responsibilitymetric, portfoliodiversitymetric
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry (
    defname, artifactkind, scopeid, kernelid,
    repotarget, destinationpath, filename,
    hash, status, active, monotoneok,
    evidencehex, signingdid, created_utc, updated_utc
)
VALUES
    ('ecosafety.vshardresponsibilityker.view.2026v1',
     'SQL',
     'RESPONSIBILITY',
     'ecosafety.ResponsibilityAxis.2026v1',
     'eco_restoration_shard',
     'db/dbresponsibilitymetrics.sql',
     'dbresponsibilitymetrics.sql',
     '',
     'FROZENACTIVE',
     1,
     1,
     '',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     datetime('now'),
     datetime('now')
    ),

    ('ecosafety.responsibilitymetric.table.2026v1',
     'SQL',
     'RESPONSIBILITY',
     'ecosafety.ResponsibilityAxis.2026v1',
     'eco_restoration_shard',
     'db/dbresponsibilitymetrics.sql',
     'dbresponsibilitymetrics.sql',
     '',
     'FROZENACTIVE',
     1,
     1,
     '',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     datetime('now'),
     datetime('now')
    ),

    ('ecosafety.portfoliodiversitymetric.table.2026v1',
     'SQL',
     'RESPONSIBILITY',
     'ecosafety.PortfolioDiversity.2026v1',
     'eco_restoration_shard',
     'db/dbresponsibilitymetrics.sql',
     'dbresponsibilitymetrics.sql',
     '',
     'FROZENACTIVE',
     1,
     1,
     '',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     datetime('now'),
     datetime('now')
    );


----------------------------------------------------------------------
-- 6. Evolution epoch and kerbandversion descriptors (non-actuating)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS evolutionepoch (
    evolutionepochid   INTEGER PRIMARY KEY AUTOINCREMENT,
    epochname          TEXT NOT NULL UNIQUE,   -- e.g. 'Phoenix-2026Q3'
    kernelid           TEXT NOT NULL,          -- KER / EcoWealth kernel set id
    kerbandversion     TEXT NOT NULL,          -- e.g. 'KERBandPhoenix2026v1'
    rohceiling         REAL NOT NULL,          -- usually 0.30
    regioncode         TEXT NOT NULL,
    effective_from_utc TEXT NOT NULL,
    effective_to_utc   TEXT,
    evidencehex        TEXT NOT NULL,
    signingdid         TEXT NOT NULL,
    created_utc        TEXT NOT NULL,
    updated_utc        TEXT NOT NULL
);

INSERT OR IGNORE INTO definitionregistry (
    defname, artifactkind, scopeid, kernelid,
    repotarget, destinationpath, filename,
    hash, status, active, monotoneok,
    evidencehex, signingdid, created_utc, updated_utc
)
VALUES
    ('evolutionepoch.Phoenix.2026Q3',
     'SQL',
     'EVOLUTIONEPOCH',
     'ecosafety.EvolutionEpoch.Phoenix.2026Q3',
     'eco_restoration_shard',
     'db/dbevolutionepoch_phx.sql',
     'dbevolutionepoch_phx.sql',
     '',
     'FROZENACTIVE',
     1,
     1,
     '',
     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
     datetime('now'),
     datetime('now')
    );


----------------------------------------------------------------------
-- 7. AI-chat facing view over the registry (vdefinitionregistry_ai)
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vdefinitionregistry_ai AS
SELECT
    d.defid,
    d.defname,
    d.artifactkind,
    d.scopeid,
    s.scopename,
    d.kernelid,
    d.repotarget,
    d.destinationpath,
    d.filename,
    d.status,
    d.active,
    d.monotoneok,
    d.hash,
    d.evidencehex,
    d.signingdid
FROM definitionregistry AS d
JOIN definitionscope   AS s
  ON d.scopeid = s.scopeid;


----------------------------------------------------------------------
-- 8. defreg-verify contract note (Rust tool to be implemented)
--
-- The Rust tool "defreg-verify" should:
--   - Enumerate files under db/, aln/, crates/ as configured.
--   - Compute allowed hashes (non-blacklisted primitives).
--   - Populate definitionregistry.hash where empty.
--   - Check that each schema/ALN/Rust module under governed dirs
--     has a corresponding definitionregistry row.
--   - Verify that active=1, monotoneok=1 is only set when:
--       * Kani harnesses pass (where applicable).
--       * Lane/KER replay harnesses report zero violations.
--   - Emit a qpudatashard-style report for CI consumption.
--
-- This contract is documented here so CI and agents know that
-- dbdefinitionregistry.sql is the canonical source of truth.
----------------------------------------------------------------------
