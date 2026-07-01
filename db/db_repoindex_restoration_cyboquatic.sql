-- filename: db/db_repoindex_restoration_cyboquatic.sql
-- destination: eco_restoration_shard/db/db_repoindex_restoration_cyboquatic.sql

PRAGMA foreign_keys = ON;

INSERT OR IGNORE INTO repofile (repoid, relpath, purpose, language, createdutc, updatedutc)
SELECT r.repoid,
       'db/db_cyboquatic_blastradius_spine.sql',
       'SCHEMASQL',
       'sqlite3',
       datetime('now'),
       datetime('now')
FROM repo AS r
WHERE r.name = 'ecorestorationshard';

INSERT OR IGNORE INTO repofile (repoid, relpath, purpose, language, createdutc, updatedutc)
SELECT r.repoid,
       'qpudatashards/particles/CyboquaticBlastRadiusShard2026v1.aln',
       'ALNSPEC',
       'ALN',
       datetime('now'),
       datetime('now')
FROM repo AS r
WHERE r.name = 'ecorestorationshard';
