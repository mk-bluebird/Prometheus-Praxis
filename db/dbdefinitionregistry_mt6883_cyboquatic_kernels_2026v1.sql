-- filename: db/dbdefinitionregistry_mt6883_cyboquatic_kernels_2026v1.sql
-- repo: mk-bluebird/eco_restoration_shard
-- destination: Eco-Fort/db/dbdefinitionregistry_mt6883_cyboquatic_kernels_2026v1.sql

PRAGMA foreign_keys = ON;

INSERT OR IGNORE INTO definitionscope (scopeid, scopename, description)
VALUES
  ('ROHMODEL',      'Risk-of-Harm models',        'RoH kernels and MT6883 healthcare corridors'),
  ('EVOLUTIONDAILY','Daily evolution surfaces',   'Daily evolution manifest surfaces'),
  ('LANEPOLICY',    'Lane policy and gating',     'Lane admissibility and gating kernels');

INSERT OR IGNORE INTO definitionregistry
  (defname, artifactkind, scopeid, kernelid,
   repotarget, destinationpath, filename,
   hash, status, active, monotoneok,
   evidencehex, signingdid, createdutc, updatedutc)
VALUES
  (
    'healthcare.roh.kernel.mt6883.2026v1',
    'SQL',
    'ROHMODEL',
    'healthcare.MT6883.RoHKernel.2026v1',
    'eco_restoration_shard',
    'db/dbrohkernel_mt6883_2026v1.sql',
    'dbrohkernel_mt6883_2026v1.sql',
    '',
    'FROZENACTIVE',
    1,
    1,
    '',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now'),
    datetime('now')
  ),
  (
    'phoenix.daily.evolution.manifest.view.2026v1',
    'SQL',
    'EVOLUTIONDAILY',
    'ecosafety.EvolutionManifest.Phoenix.2026v1',
    'eco_restoration_shard',
    'db/dbphoenix_daily_manifest_rohker_2026v1.sql',
    'dbphoenix_daily_manifest_rohker_2026v1.sql',
    '',
    'FROZENACTIVE',
    1,
    1,
    '',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now'),
    datetime('now')
  ),
  (
    'ecosafety.lane.admissibility.kernel.2026v1',
    'ALN',
    'LANEPOLICY',
    'ecosafety.LaneAdmissibilityKernel.2026v1',
    'eco_restoration_shard',
    'aln/LaneAdmissibilityKernel2026v1.aln',
    'LaneAdmissibilityKernel2026v1.aln',
    '',
    'FROZENACTIVE',
    1,
    1,
    '',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now'),
    datetime('now')
  ),
  (
    'healthcare.roh.kernel.mt6883.aln.2026v1',
    'ALN',
    'ROHMODEL',
    'healthcare.MT6883.RoHKernel.2026v1',
    'eco_restoration_shard',
    'aln/RoHKernelMT6883_2026v1.aln',
    'RoHKernelMT6883_2026v1.aln',
    '',
    'FROZENACTIVE',
    1,
    1,
    '',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now'),
    datetime('now')
  ),
  (
    'phoenix.daily.evolution.manifest.aln.2026v1',
    'ALN',
    'EVOLUTIONDAILY',
    'ecosafety.EvolutionManifest.Phoenix.2026v1',
    'eco_restoration_shard',
    'aln/PhoenixDailyEvolutionManifest2026v1.aln',
    'PhoenixDailyEvolutionManifest2026v1.aln',
    '',
    'FROZENACTIVE',
    1,
    1,
    '',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now'),
    datetime('now')
  );
