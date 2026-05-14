-- filename db_repofile_from_definition_registry2026v1.sql
-- destination Eco-Fort/db/db_repofile_from_definition_registry2026v1.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 34. Insert repofile rows for DefinitionRegistry2026v1 entries
-------------------------------------------------------------------------------

-- Existing repofile schema (from constellation spine):
--   repofile (
--     fileid      INTEGER PRIMARY KEY AUTOINCREMENT,
--     repoid      INTEGER NOT NULL REFERENCES repo(repoid) ON DELETE CASCADE,
--     relpath     TEXT NOT NULL,
--     filename    TEXT NOT NULL,
--     ext         TEXT NOT NULL,
--     filekind    TEXT NOT NULL CHECK (filekind IN ('ALN','CSV','RUST','CPP','CSHARP','LUA','KOTLIN','JS','HTML','DOC','OTHER')),
--     dirclass    TEXT NOT NULL CHECK (dirclass IN ('QPUDATASHARD','PARTICLE','SCHEMA','SRC','DOC','CONFIG','OTHER')),
--     sha256hex   TEXT,
--     bytessize   INTEGER,
--     lastcommitsha TEXT,
--     lastupdatedutc TEXT
--   )

-- Assumes definition_registry table as previously defined, with alnfile, sqlfile, repotarget.

WITH defs AS (
    SELECT
        dr.particlename,
        dr.alnfile,
        dr.sqlfile,
        dr.repotarget,
        repo.repoid
    FROM definition_registry dr
    JOIN repo
      ON repo.name = dr.repotarget
    WHERE dr.repotarget = 'eco_restoration_shard'
), aln_files AS (
    SELECT
        d.repoid,
        d.alnfile AS path
    FROM defs d
    WHERE d.alnfile IS NOT NULL
), sql_files AS (
    SELECT
        d.repoid,
        d.sqlfile AS path
    FROM defs d
    WHERE d.sqlfile IS NOT NULL
), all_files AS (
    SELECT * FROM aln_files
    UNION ALL
    SELECT * FROM sql_files
)
INSERT INTO repofile (
    repoid,
    relpath,
    filename,
    ext,
    filekind,
    dirclass,
    sha256hex,
    bytessize,
    lastcommitsha,
    lastupdatedutc
)
SELECT
    f.repoid,
    substr(f.path, 1, length(f.path) - length(substr(f.path, instr(f.path, '/', -1)))) AS relpath,
    substr(f.path, instr(f.path, '/', -1) + 1) AS filename,
    substr(f.path, instr(f.path, '.', -1) + 1) AS ext,
    CASE
        WHEN substr(f.path, instr(f.path, '.', -1) + 1) = 'aln' THEN 'ALN'
        WHEN substr(f.path, instr(f.path, '.', -1) + 1) = 'sql' THEN 'OTHER'
        ELSE 'OTHER'
    END AS filekind,
    CASE
        WHEN f.path LIKE 'aln/%' THEN 'SCHEMA'
        WHEN f.path LIKE 'db/%' THEN 'CONFIG'
        ELSE 'OTHER'
    END AS dirclass,
    NULL AS sha256hex,
    NULL AS bytessize,
    NULL AS lastcommitsha,
    datetime('now') AS lastupdatedutc
FROM all_files f;
