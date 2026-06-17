-- filename: db/dbdefinitionregistry_phx_dailyevolutionmanifest.sql
-- destination: Eco-Fort/db/dbdefinitionregistry_phx_dailyevolutionmanifest.sql
-- description: DefinitionRegistry seed rows for Phoenix daily evolution manifest.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Ensure a scope exists for ECOWEALTH and PHOENIX_EVOLUTION
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionscope (scopeid, scopename, description)
VALUES
    ('ECOWEALTH', 'ECOWEALTH', 'EcoWealth kernels, statements, and views'),
    ('PHX_EVOLUTION', 'PHX_EVOLUTION', 'Phoenix-AZ-US daily evolution manifest surfaces');

----------------------------------------------------------------------
-- 2. Register the Phoenix daily evolution manifest view
--
-- logicalname: ecowealth.phx.daily_evolution_manifest.view.2026v1
-- versiontag : 2026v1
-- status     : FROZEN_ACTIVE
-- linkedtable: vphx_daily_evolution_manifest
-- linkedaln  : StewardEcoWealthStatement2026v1.aln (indirectly via vecowealthview)
-- docpath    : docs/phx/daily_evolution_manifest.md (to be created separately)
--
-- The hash column should be filled with the canonical hex hash of this
-- dbphx_dailyevolutionmanifest.sql file once frozen.
----------------------------------------------------------------------

INSERT OR REPLACE INTO definitionregistry (
    logicalname,
    versiontag,
    hash,
    status,
    scopeid,
    linkedtable,
    linkedaln,
    docpath
) VALUES (
    'ecowealth.phx.daily_evolution_manifest.view.2026v1',
    '2026v1',
    '0xTODOFILLHASHPHXDAILYEVO2026V1',
    'FROZEN_ACTIVE',
    'PHX_EVOLUTION',
    'vphx_daily_evolution_manifest',
    'StewardEcoWealthStatement2026v1.aln',
    'docs/phx/daily_evolution_manifest.md'
);
