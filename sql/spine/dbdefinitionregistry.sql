-- filename: dbdefinitionregistry.sql
-- destination: eco_restoration_shard/sql/spine/dbdefinitionregistry.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS definitionregistry (
    def_id              INTEGER PRIMARY KEY AUTOINCREMENT,
    -- Stable name for the definition bundle, e.g. StewardEcoWealthStatement, KerResidualCore.
    def_name            TEXT NOT NULL,
    -- Short type: SQL, RUST_CRATE, ALN_PARTICLE, CONFIG, WORKFLOW.
    def_type            TEXT NOT NULL CHECK (
        def_type IN (
            'SQL',
            'RUST_CRATE',
            'ALN_PARTICLE',
            'CONFIG',
            'WORKFLOW',
            'CSV',
            'DOC'
        )
    ),
    -- Logical role or plane, e.g. 'eco_wealth', 'ker_residual', 'healthcare_mt6883'.
    role_plane          TEXT NOT NULL,
    -- Repository target in the EcoNet constellation, e.g. 'eco_restoration_shard', 'EcoNet-CEIM-PhoenixWater'.
    repo_target         TEXT NOT NULL,
    -- Relative path inside repo (directory + filename).
    relpath             TEXT NOT NULL,
    -- Optional Rust crate name or SQL schema tag.
    crate_or_schema     TEXT,
    -- Version label, e.g. 'v1', '2026Q2'.
    version_label       TEXT NOT NULL,
    -- Whether this definition is currently active.
    active              INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    -- Optional upstream maintainer label, e.g. 'mk-bluebird', 'Doctor0Evil'.
    maintainer          TEXT,
    created_utc         TEXT NOT NULL,
    updated_utc         TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_definitionregistry_name
    ON definitionregistry(def_name, version_label);

CREATE INDEX IF NOT EXISTS idx_definitionregistry_repo
    ON definitionregistry(repo_target, relpath, active);


-- StewardEcoWealthStatement core definition seeds.

INSERT INTO definitionregistry (
    def_name,
    def_type,
    role_plane,
    repo_target,
    relpath,
    crate_or_schema,
    version_label,
    active,
    maintainer,
    created_utc,
    updated_utc
) VALUES
    (
        'StewardEcoWealthStatement',
        'SQL',
        'eco_wealth',
        'eco_restoration_shard',
        'sql/eco_wealth/dbsteward_eco_wealth_statement.sql',
        'eco_wealth',
        'v1',
        1,
        'mk-bluebird',
        '2026-05-17T18:30:00Z',
        '2026-05-17T18:30:00Z'
    ),
    (
        'StewardEcoWealthStatementViewShard',
        'SQL',
        'eco_wealth',
        'eco_restoration_shard',
        'sql/eco_wealth/dbsteward_eco_wealth_vshard.sql',
        'eco_wealth',
        'v1',
        1,
        'mk-bluebird',
        '2026-05-17T18:30:00Z',
        '2026-05-17T18:30:00Z'
    ),
    (
        'KerResidualCore',
        'RUST_CRATE',
        'ker_residual',
        'eco_restoration_shard',
        'crates/kerresidual',
        'kerresidual',
        'v1',
        1,
        'mk-bluebird',
        '2026-05-17T18:30:00Z',
        '2026-05-17T18:30:00Z'
    ),
    (
        'BioscaleFairnessValidator',
        'RUST_CRATE',
        'healthcare_mt6883',
        'eco_restoration_shard',
        'crates/bioscale-fairness-validator',
        'bioscale_fairness_validator',
        'v1',
        1,
        'mk-bluebird',
        '2026-05-17T18:30:00Z',
        '2026-05-17T18:30:00Z'
    ),
    (
        'DailyEvolutionDaemonConfig',
        'CONFIG',
        'ker_residual',
        'eco_restoration_shard',
        'config/daily_evolution_daemon.toml',
        NULL,
        'v1',
        1,
        'mk-bluebird',
        '2026-05-17T18:30:00Z',
        '2026-05-17T18:30:00Z'
    ),
    (
        'KerCIWorkflow',
        'WORKFLOW',
        'ker_residual',
        'eco_restoration_shard',
        '.github/workflows/ker_ci.yml',
        NULL,
        'v1',
        1,
        'mk-bluebird',
        '2026-05-17T18:30:00Z',
        '2026-05-17T18:30:00Z'
    );
