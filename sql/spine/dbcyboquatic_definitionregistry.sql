-- filename: dbcyboquatic_definitionregistry.sql
-- destination: eco_restoration_shard/sql/spine/dbcyboquatic_definitionregistry.sql

PRAGMA foreign_keys = ON;

-- Reuse existing definitionregistry if present; otherwise create it.

CREATE TABLE IF NOT EXISTS definitionregistry (
    defid        INTEGER PRIMARY KEY AUTOINCREMENT,
    defname      TEXT NOT NULL,
    deftype      TEXT NOT NULL CHECK (deftype IN ('SQL','RUSTCRATE','ALNPARTICLE','CONFIG','WORKFLOW','CSV','DOC')),
    roleplane    TEXT NOT NULL,
    repotarget   TEXT NOT NULL,
    relpath      TEXT NOT NULL,
    crateorschema TEXT,
    versionlabel TEXT NOT NULL,
    active       INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    maintainer   TEXT,
    createdutc   TEXT NOT NULL,
    updatedutc   TEXT NOT NULL
);

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
    'CyboquaticEcoMachineryIndex',
    'SQL',
    'cyboquatic-eco',
    'eco_restoration_shard',
    'sql/spine/dbcyboquatic_ecomachinery.sql',
    'cyboquatic-eco-index',
    'v1',
    1,
    'mk-bluebird',
    '2026-06-14T23:34:00Z',
    '2026-06-14T23:34:00Z'
),
(
    'CyboquaticEcoSpineCdylib',
    'RUSTCRATE',
    'cyboquatic-eco',
    'eco_restoration_shard',
    'src/cyboquatic_eco_spine.rs',
    'cyboquatic-eco-spine',
    'v1',
    1,
    'mk-bluebird',
    '2026-06-14T23:34:00Z',
    '2026-06-14T23:34:00Z'
),
(
    'CyboquaticEcoOverlayLua',
    'DOC',
    'cyboquatic-eco',
    'eco_restoration_shard',
    'lua/cyboquatic_eco_overlay.lua',
    NULL,
    'v1',
    1,
    'mk-bluebird',
    '2026-06-14T23:34:00Z',
    '2026-06-14T23:34:00Z'
),
(
    'CyboquaticEcoOverlayAndroid',
    'DOC',
    'cyboquatic-eco',
    'eco_restoration_shard',
    'Cyboquatics-Android/android/app/src/main/java/org/econet/CyboquaticEcoOverlay.kt',
    NULL,
    'v1',
    1,
    'mk-bluebird',
    '2026-06-14T23:34:00Z',
    '2026-06-14T23:34:00Z'
);
