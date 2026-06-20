-- filename: db/dbdefinitionregistry_cyboquatic_2026v1.sql
-- repo: mk-bluebird/eco_restoration_shard
-- destination: Eco-Fort/db/dbdefinitionregistry_cyboquatic_2026v1.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Ensure Cyboquatic-related scopes exist in definitionscope
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionscope (scopeid, scopename, description)
VALUES
  ('KERKERNEL',     'KER kernel surfaces',           'Core K,E,R,Vt kernels and per-plane residual surfaces'),
  ('ECOWEALTH',     'EcoWealth kernels',             'EcoUnit, EcoWealth and restorative wealth kernels'),
  ('LANEPOLICY',    'Lane policy and gating',        'Lane admissibility, lane harnesses, and gating surfaces'),
  ('ROHMODEL',      'Risk-of-harm models',           'RoH and healthcare risk kernels and overlays'),
  ('TOPOLOGYAUDIT', 'Topology and blast audit',      'Adjacency, blast-radius and restoration-aware topology'),
  ('CYBOQUATIC',    'Cyboquatic domain kernels',     'Cyboquatic carbon-negative and restoration kernels'),
  ('VIEWGRAMMAR',   'View grammar surfaces',         'Canonical, read-only views used by agents and CI'),
  ('EVOLUTIONDAILY','Daily evolution surfaces',      'Daily evolution and restoration surfaces per region/steward');

----------------------------------------------------------------------
-- 2. CyboquaticEcoPlot core table (ecoperjoule + carbonnegativeok)
-- logicalname: cyboquatic.ecoplot.core.table.2026v1
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry
  (logicalname,
   versiontag,
   scopeid,
   hash,
   status,
   linkedtable,
   linkedaln,
   docpath)
VALUES
  (
    'cyboquatic.ecoplot.core.table.2026v1',
    '2026v1',
    'CYBOQUATIC',
    lower(hex(randomblob(16))),
    'FROZENACTIVE',
    'CyboquaticEcoPlot',
    'CyboquaticEcoPlotCore2026v1.aln',
    'docs/cyboquatic/CyboquaticEcoPlotCore2026v1.md'
  );

----------------------------------------------------------------------
-- 3. Cyboquatic carbon-negative PROD view
-- logicalname: cyboquatic.ecoplot.prodcarbonnegative.view.2026v1
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry
  (logicalname,
   versiontag,
   scopeid,
   hash,
   status,
   linkedtable,
   linkedaln,
   docpath)
VALUES
  (
    'cyboquatic.ecoplot.prodcarbonnegative.view.2026v1',
    '2026v1',
    'LANEPOLICY',
    lower(hex(randomblob(16))),
    'FROZENACTIVE',
    'cyboquaticecoplotprodcarbonnegative',
    'CyboquaticEcoPlotProdCarbonNegative2026v1.aln',
    'docs/cyboquatic/CyboquaticEcoPlotProdCarbonNegative2026v1.md'
  );

----------------------------------------------------------------------
-- 4. Cyboquatic restoration surface table
-- logicalname: cyboquatic.restoration.surface.table.2026v1
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry
  (logicalname,
   versiontag,
   scopeid,
   hash,
   status,
   linkedtable,
   linkedaln,
   docpath)
VALUES
  (
    'cyboquatic.restoration.surface.table.2026v1',
    '2026v1',
    'CYBOQUATIC',
    lower(hex(randomblob(16))),
    'FROZENACTIVE',
    'CyboquaticRestorationSurface',
    'CyboquaticRestorationSurface2026v1.aln',
    'docs/cyboquatic/CyboquaticRestorationSurface2026v1.md'
  );

----------------------------------------------------------------------
-- 5. Cyboquatic restoration nodes view (blast-aware)
-- logicalname: cyboquatic.restoration.nodes.view.2026v1
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry
  (logicalname,
   versiontag,
   scopeid,
   hash,
   status,
   linkedtable,
   linkedaln,
   docpath)
VALUES
  (
    'cyboquatic.restoration.nodes.view.2026v1',
    '2026v1',
    'TOPOLOGYAUDIT',
    lower(hex(randomblob(16))),
    'FROZENACTIVE',
    'cyboquaticrestorationnodes',
    'CyboquaticRestorationNodes2026v1.aln',
    'docs/cyboquatic/CyboquaticRestorationNodes2026v1.md'
  );

----------------------------------------------------------------------
-- 6. Plane weights for Cyboquatic planes
-- logicalname: ecosafety.planeweights.cyboquatic.planes.2026v1
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry
  (logicalname,
   versiontag,
   scopeid,
   hash,
   status,
   linkedtable,
   linkedaln,
   docpath)
VALUES
  (
    'ecosafety.planeweights.cyboquatic.planes.2026v1',
    '2026v1',
    'KERKERNEL',
    lower(hex(randomblob(16))),
    'FROZENACTIVE',
    'planeweights',
    'PlaneWeightsCyboquaticPlanes2026v1.aln',
    'docs/cyboquatic/PlaneWeightsCyboquaticPlanes2026v1.md'
  );

----------------------------------------------------------------------
-- 7. Cyboquatic KER window with per-plane breakdown
-- logicalname: cyboquatic.ker.window.planes.view.2026v1
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry
  (logicalname,
   versiontag,
   scopeid,
   hash,
   status,
   linkedtable,
   linkedaln,
   docpath)
VALUES
  (
    'cyboquatic.ker.window.planes.view.2026v1',
    '2026v1',
    'KERKERNEL',
    lower(hex(randomblob(16))),
    'FROZENACTIVE',
    'vcyboquaticwindowwithplanes',
    'CyboquaticKerWindowPlanes2026v1.aln',
    'docs/cyboquatic/CyboquaticKerWindowPlanes2026v1.md'
  );

----------------------------------------------------------------------
-- 8. Cyboquatic blast + restoration adjacency view
-- logicalname: cyboquatic.blast.restoration.view.2026v1
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry
  (logicalname,
   versiontag,
   scopeid,
   hash,
   status,
   linkedtable,
   linkedaln,
   docpath)
VALUES
  (
    'cyboquatic.blast.restoration.view.2026v1',
    '2026v1',
    'TOPOLOGYAUDIT',
    lower(hex(randomblob(16))),
    'FROZENACTIVE',
    'vcyboquaticblastradiusrestoration',
    'CyboquaticBlastRestorationView2026v1.aln',
    'docs/cyboquatic/CyboquaticBlastRestorationView2026v1.md'
  );
