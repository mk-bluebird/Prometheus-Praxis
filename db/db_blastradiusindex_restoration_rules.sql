-- filename db_blastradiusindex_restoration_rules.sql
-- destination Eco-Fort/db/db_blastradiusindex_restoration_rules.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 40. MAR-aware blastradiusindex restoration rules
-------------------------------------------------------------------------------
-- Existing blastradiusindex (assumed):
--   blastradiusindex(
--     radius_id INTEGER PRIMARY KEY AUTOINCREMENT,
--     nodeid    TEXT NOT NULL,
--     region    TEXT NOT NULL,
--     marplane  TEXT NOT NULL,
--     blastradiusm REAL NOT NULL
--   );

ALTER TABLE blastradiusindex
ADD COLUMN restorationradiusm REAL;

ALTER TABLE blastradiusindex
ADD COLUMN restorationok INTEGER NOT NULL DEFAULT 0
CHECK (restorationok IN (0,1));

-- ENGINE-repo binding table (assumed from econetrepoindex):
--   econetrepoindex(reponame, roleband, ...)

-- Helper view: ENGINE repos targeting Phoenix MAR nodes
CREATE VIEW IF NOT EXISTS v_engine_phoenix_mar_binding AS
SELECT
    eri.reponame,
    eri.githubslug,
    eri.roleband,
    bri.nodeid,
    bri.region,
    bri.marplane,
    bri.blastradiusm,
    bri.restorationradiusm,
    bri.restorationok
FROM econetrepoindex eri
JOIN blastradiusindex bri
  ON bri.region = 'Phoenix-AZ'
WHERE eri.roleband = 'ENGINE';

-- Trigger: reject ENGINE binding when restorationok = 0 for Phoenix MAR nodes
CREATE TRIGGER IF NOT EXISTS trg_engine_binding_restoration_guard
BEFORE INSERT ON econetrepoindex
WHEN NEW.roleband = 'ENGINE'
AND EXISTS (
    SELECT 1
    FROM blastradiusindex bri
    WHERE bri.region = 'Phoenix-AZ'
      AND bri.restorationok = 0
)
BEGIN
    SELECT RAISE(ABORT, 'ENGINE repo binding rejected: restorationok=0 for Phoenix MAR nodes');
END;
