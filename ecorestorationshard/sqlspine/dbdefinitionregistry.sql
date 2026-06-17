-- FILE: ecorestorationshard/sqlspine/dbdefinitionregistry.sql
-- DESTINATION: ecorestorationshard/sqlspine/dbdefinitionregistry.sql
-- REPO-TARGET: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS definitionregistry (
    defid INTEGER PRIMARY KEY AUTOINCREMENT,
    defname TEXT NOT NULL,
    deftype TEXT NOT NULL CHECK (deftype IN ('SQL','RUSTCRATE','ALNPARTICLE','CONFIG','WORKFLOW','CSV','DOC')),
    roleplane TEXT NOT NULL,
    repotarget TEXT NOT NULL,
    relpath TEXT NOT NULL,
    crateorschema TEXT,
    versionlabel TEXT NOT NULL,
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    maintainer TEXT,
    createdutc TEXT NOT NULL,
    updatedutc TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_definitionregistry_name
    ON definitionregistry(defname, versionlabel);

CREATE INDEX IF NOT EXISTS idx_definitionregistry_repo
    ON definitionregistry(repotarget, relpath, active);

INSERT INTO definitionregistry (
    defname,
    deftype,
    roleplane,
    repotarget,
    relpath,
    crateorschema,
    versionlabel,
    active,
    maintainer,
    createdutc,
    updatedutc
) VALUES
    (
        'StewardEcoWealthStatement',
        'SQL',
        'ecowealth',
        'ecorestorationshard',
        'sqlecowealth/dbstewardecowealthstatement.sql',
        'ecowealth',
        'v1',
        1,
        'mk-bluebird',
        '2026-05-17T18:30:00Z',
        '2026-05-17T18:30:00Z'
    ),
    (
        'StewardEcoWealthStatementViewShard',
        'SQL',
        'ecowealth',
        'ecorestorationshard',
        'sqlecowealth/dbstewardecowealthvshard.sql',
        'ecowealth',
        'v1',
        1,
        'mk-bluebird',
        '2026-05-17T18:30:00Z',
        '2026-05-17T18:30:00Z'
    ),
    (
        'KerResidualCore',
        'RUSTCRATE',
        'kerresidual',
        'ecorestorationshard',
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
        'RUSTCRATE',
        'healthcaremt6883',
        'ecorestorationshard',
        'crates/bioscale-fairness-validator',
        'bioscale-fairness-validator',
        'v1',
        1,
        'mk-bluebird',
        '2026-05-17T18:30:00Z',
        '2026-05-17T18:30:00Z'
    ),
    (
        'DailyEvolutionDaemonConfig',
        'CONFIG',
        'kerresidual',
        'ecorestorationshard',
        'config/dailyevolutiondaemon.toml',
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
        'kerresidual',
        'ecorestorationshard',
        '.github/workflows/kerci.yml',
        NULL,
        'v1',
        1,
        'mk-bluebird',
        '2026-05-17T18:30:00Z',
        '2026-05-17T18:30:00Z'
    );
