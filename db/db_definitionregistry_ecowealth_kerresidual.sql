-- filename: eco_restoration_shard/db/db_definitionregistry_ecowealth_kerresidual.sql
-- destination: eco_restoration_shard/db/db_definitionregistry_ecowealth_kerresidual.sql

PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

INSERT INTO definitionregistry
    (logicalname,
     versiontag,
     hash,
     status,
     linkedtable,
     linkedaln,
     docpath)
VALUES
    -- kerresidual Rust module (shared KER residual kernel for EcoWealth).
    ('kerresidual.ecowealth.kernel',
     '2026v1',
     'hex-kerresidual-ecowealth-2026v1',
     'FROZEN_ACTIVE',
     'residualkernel',
     'aln/EcoWealthKernel2026v1.aln',
     'doc/EcoWealthKernel2026v1.md'),

    -- vshardtopologyker view (topology plane + representation floor).
    ('vshardtopologyker',
     '2026v1',
     'hex-vshardtopologyker-2026v1',
     'FROZEN_ACTIVE',
     'vshardtopologyker',
     NULL,
     'db/db_vshardtopologyker.sql'),

    -- vecowealthview view (steward eco-wealth portfolio).
    ('vecowealthview',
     '2026v1',
     'hex-vecowealthview-2026v1',
     'FROZEN_ACTIVE',
     'vecowealthview',
     NULL,
     'eco_restoration_shard/db/db_vecowealthview.sql'),

    -- SensorTrustPlane2026v1.aln (ALN contract for dataquality sensor trust).
    ('SensorTrustPlane',
     '2026v1',
     'hex-SensorTrustPlane2026v1',
     'FROZEN_ACTIVE',
     'planeweights',
     'aln/SensorTrustPlane2026v1.aln',
     'doc/SensorTrustPlane2026v1.md'),

    -- EcoWealthKernel2026v1.aln (ALN ecowealth kernel, EcoUnit definition).
    ('EcoWealthKernel',
     '2026v1',
     'hex-EcoWealthKernel2026v1',
     'FROZEN_ACTIVE',
     'kerdefinition',
     'aln/EcoWealthKernel2026v1.aln',
     'doc/EcoWealthKernel2026v1.md');

COMMIT;
