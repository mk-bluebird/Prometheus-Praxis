-- filename: db/dbmcp_cyboquatic_2026v1.sql
-- repo: mk-bluebird/eco_restoration_shard
-- destination: Eco-Fort/db/dbmcp_cyboquatic_2026v1.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Ensure eco_restoration_shard is registered in mcprepo
----------------------------------------------------------------------

INSERT OR IGNORE INTO mcprepo
  (reponame,
   githubslug,
   roleband,
   primaryplane,
   lanedefault,
   nonactuatingonly,
   didowner,
   createdutc)
VALUES
  (
    'eco_restoration_shard',
    'mk-bluebird/eco_restoration_shard',
    'RESEARCH',
    'CARBON',
    'RESEARCH',
    1,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now')
  );

----------------------------------------------------------------------
-- 2. mcpfile entries for Cyboquatic SQL/ALN artifacts
--    Note: repoid is resolved via subquery on mcprepo.
----------------------------------------------------------------------

INSERT OR IGNORE INTO mcpfile
  (repoid,
   relpath,
   filekind,
   filerole,
   planebands,
   createdutc)
SELECT
  r.repoid,
  'db/dbcyboquatic_ecoplot_2026v1.sql',
  'SQL',
  'SCHEMA',
  '["CARBON","ENERGY","CYBOQUATIC"]',
  datetime('now')
FROM mcprepo AS r
WHERE r.reponame = 'eco_restoration_shard';

INSERT OR IGNORE INTO mcpfile
  (repoid,
   relpath,
   filekind,
   filerole,
   planebands,
   createdutc)
SELECT
  r.repoid,
  'db/dbcyboquatic_restoration_2026v1.sql',
  'SQL',
  'SCHEMA',
  '["HYDRO","CARBON","RESTORATION","CYBOQUATIC"]',
  datetime('now')
FROM mcprepo AS r
WHERE r.reponame = 'eco_restoration_shard';

INSERT OR IGNORE INTO mcpfile
  (repoid,
   relpath,
   filekind,
   filerole,
   planebands,
   createdutc)
SELECT
  r.repoid,
  'db/dbplaneweights_cyboquatic_2026v1.sql',
  'SQL',
  'SCHEMA',
  '["CARBON","HYDRO","RESTORATION","CYBOQUATIC"]',
  datetime('now')
FROM mcprepo AS r
WHERE r.reponame = 'eco_restoration_shard';

INSERT OR IGNORE INTO mcpfile
  (repoid,
   relpath,
   filekind,
   filerole,
   planebands,
   createdutc)
SELECT
  r.repoid,
  'db/dbcyboquatic_ker_window_planes_2026v1.sql',
  'SQL',
  'VIEW',
  '["CARBON","HYDRO","CYBOQUATIC"]',
  datetime('now')
FROM mcprepo AS r
WHERE r.reponame = 'eco_restoration_shard';

INSERT OR IGNORE INTO mcpfile
  (repoid,
   relpath,
   filekind,
   filerole,
   planebands,
   createdutc)
SELECT
  r.repodid,
  'db/dbcyboquatic_blast_restoration_view_2026v1.sql',
  'SQL',
  'VIEW',
  '["BLAST","HYDRO","CYBOQUATIC"]',
  datetime('now')
FROM mcprepo AS r
WHERE r.reponame = 'eco_restoration_shard';

----------------------------------------------------------------------
-- 3. mcptool entries for Cyboquatic diagnostic tools
----------------------------------------------------------------------

INSERT OR IGNORE INTO mcptool
  (repoid,
   fileid,
   toolname,
   toolkind,
   resourcemode,
   roleband,
   lanedefault,
   planebands,
   neuroflag,
   stewarddid,
   createdutc)
SELECT
  r.repoid,
  f.fileid,
  'cyboquatic_ecoplot_query',
  'SQLQUERY',
  'READONLY',
  'RESEARCH',
  'RESEARCH',
  '["CARBON","ENERGY","CYBOQUATIC"]',
  0,
  'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
  datetime('now')
FROM mcprepo AS r
JOIN mcpfile AS f
  ON f.repoid = r.repoid
 AND f.relpath = 'db/dbcyboquatic_ecoplot_2026v1.sql'
WHERE r.reponame = 'eco_restoration_shard';

INSERT OR IGNORE INTO mcptool
  (repoid,
   fileid,
   toolname,
   toolkind,
   resourcemode,
   roleband,
   lanedefault,
   planebands,
   neuroflag,
   stewarddid,
   createdutc)
SELECT
  r.repoid,
  f.fileid,
  'cyboquatic_carbonnegative_nodes',
  'SQLQUERY',
  'READONLY',
  'RESEARCH',
  'RESEARCH',
  '["CARBON","LANE","CYBOQUATIC"]',
  0,
  'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
  datetime('now')
FROM mcprepo AS r
JOIN mcpfile AS f
  ON f.repoid = r.repoid
 AND f.relpath = 'db/dbcyboquatic_ecoplot_2026v1.sql'
WHERE r.reponame = 'eco_restoration_shard';

INSERT OR IGNORE INTO mcptool
  (repoid,
   fileid,
   toolname,
   toolkind,
   resourcemode,
   roleband,
   lanedefault,
   planebands,
   neuroflag,
   stewarddid,
   createdutc)
SELECT
  r.repoid,
  f.fileid,
  'cyboquatic_restoration_surface',
  'SQLQUERY',
  'READONLY',
  'RESEARCH',
  'RESEARCH',
  '["HYDRO","CARBON","RESTORATION","CYBOQUATIC"]',
  0,
  'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
  datetime('now')
FROM mcprepo AS r
JOIN mcpfile AS f
  ON f.repoid = r.repoid
 AND f.relpath = 'db/dbcyboquatic_restoration_2026v1.sql'
WHERE r.reponame = 'eco_restoration_shard';

INSERT OR IGNORE INTO mcptool
  (repoid,
   fileid,
   toolname,
   toolkind,
   resourcemode,
   roleband,
   lanedefault,
   planebands,
   neuroflag,
   stewarddid,
   createdutc)
SELECT
  r.repoid,
  f.fileid,
  'cyboquatic_restoration_status',
  'SQLQUERY',
  'READONLY',
  'RESEARCH',
  'RESEARCH',
  '["HYDRO","CARBON","BLAST","LANE"]',
  0,
  'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
  datetime('now')
FROM mcprepo AS r
JOIN mcpfile AS f
  ON f.repoid = r.repoid
 AND f.relpath = 'db/dbcyboquatic_restoration_2026v1.sql'
WHERE r.reponame = 'eco_restoration_shard';

INSERT OR IGNORE INTO mcptool
  (repoid,
   fileid,
   toolname,
   toolkind,
   resourcemode,
   roleband,
   lanedefault,
   planebands,
   neuroflag,
   stewarddid,
   createdutc)
SELECT
  r.repoid,
  f.fileid,
  'cyboquatic_ker_plane_breakdown',
  'SQLQUERY',
  'READONLY',
  'RESEARCH',
  'RESEARCH',
  '["CARBON","HYDRO","CYBOQUATIC"]',
  0,
  'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
  datetime('now')
FROM mcprepo AS r
JOIN mcpfile AS f
  ON f.repoid = r.repoid
 AND f.relpath = 'db/dbcyboquatic_ker_window_planes_2026v1.sql'
WHERE r.reponame = 'eco_restoration_shard';

INSERT OR IGNORE INTO mcptool
  (repoid,
   fileid,
   toolname,
   toolkind,
   resourcemode,
   roleband,
   lanedefault,
   planebands,
   neuroflag,
   stewarddid,
   createdutc)
SELECT
  r.repoid,
  f.fileid,
  'cyboquatic_blast_restoration_neighbors',
  'SQLQUERY',
  'READONLY',
  'RESEARCH',
  'RESEARCH',
  '["BLAST","HYDRO","CYBOQUATIC"]',
  0,
  'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
  datetime('now')
FROM mcprepo AS r
JOIN mcpfile AS f
  ON f.repoid = r.repoid
 AND f.relpath = 'db/dbcyboquatic_blast_restoration_view_2026v1.sql'
WHERE r.reponame = 'eco_restoration_shard';
