-- filename dbdblargeparticlefile.sql
-- destination Eco-Fort/db/dblargeparticlefile.sql
-- repo-target github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-----------------------------------------------------------------------
-- 1. Large particle file manifest (one row per logical large file)
-----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS largeparticlefile (
    fileid                 TEXT PRIMARY KEY,                -- stable logical ID
    assetclass             TEXT NOT NULL,                   -- e.g. CODE, DATASET, MODEL, VIDEO
    logicalname            TEXT NOT NULL,                   -- human-friendly label
    region                 TEXT NOT NULL,                   -- e.g. Phoenix-AZ
    ecoscope               TEXT NOT NULL,                   -- e.g. GOVERNANCE, RESTORATION, KNOWLEDGE
    ownerbostrom           TEXT NOT NULL,                   -- owning host identity
    repotarget             TEXT NOT NULL,                   -- GitHub slug or repo identifier
    relpath                TEXT NOT NULL,                   -- repo-relative path for this file
    totallengthbytes       INTEGER NOT NULL,                -- full file length in bytes
    chunksizebytes         INTEGER NOT NULL,                -- nominal chunk size in bytes
    chunkcount             INTEGER NOT NULL,                -- number of chunks
    filemerkleroothex      TEXT NOT NULL,                   -- Merkle root over ordered chunk hashes
    filehashalgo           TEXT NOT NULL,                   -- e.g. ORGANIC256
    contenttype            TEXT,                            -- MIME-type hint
    createdutc             TEXT NOT NULL,                   -- ISO8601 UTC
    updatedutc             TEXT NOT NULL,                   -- ISO8601 UTC
    lane                   TEXT NOT NULL DEFAULT 'RESEARCH',-- RESEARCH/EXPPROD/PROD
    kerdeployable          INTEGER NOT NULL DEFAULT 0,      -- 0/1 ready for KER-governed use
    evidencehex            TEXT,                            -- optional evidence bundle
    signingdid             TEXT,                            -- DID that signed the manifest
    CHECK (chunksizebytes > 0),
    CHECK (chunkcount >= 0),
    CHECK (totallengthbytes >= 0),
    CHECK (kerdeployable IN (0,1)),
    CHECK (lane IN ('RESEARCH','EXPPROD','PROD')),
    CHECK (filehashalgo IN ('ORGANIC256'))
);

CREATE INDEX IF NOT EXISTS idx_largeparticlefile_region_ecoscope
    ON largeparticlefile(region, ecoscope);

CREATE INDEX IF NOT EXISTS idx_largeparticlefile_repotarget_relpath
    ON largeparticlefile(repotarget, relpath);

CREATE INDEX IF NOT EXISTS idx_largeparticlefile_lane
    ON largeparticlefile(lane);

-----------------------------------------------------------------------
-- 2. Chunk table (one row per fixed-size chunk)
-----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS largeparticlechunk (
    fileid          TEXT NOT NULL REFERENCES largeparticlefile(fileid)
                        ON DELETE CASCADE,
    chunkindex      INTEGER NOT NULL,           -- 0-based index
    offsetbytes     INTEGER NOT NULL,           -- starting offset in file
    lengthbytes     INTEGER NOT NULL,           -- chunk length in bytes
    chunkhashhex    TEXT NOT NULL,             -- H(chunk)
    filehashalgo    TEXT NOT NULL,             -- must match parent filehashalgo
    storageuri      TEXT,                      -- location of chunk bytes
    createdutc      TEXT NOT NULL,
    updatedutc      TEXT NOT NULL,
    PRIMARY KEY (fileid, chunkindex),
    CHECK (chunkindex >= 0),
    CHECK (offsetbytes >= 0),
    CHECK (lengthbytes > 0),
    CHECK (filehashalgo IN ('ORGANIC256'))
);

CREATE INDEX IF NOT EXISTS idx_largeparticlechunk_fileid_offset
    ON largeparticlechunk(fileid, offsetbytes);

CREATE INDEX IF NOT EXISTS idx_largeparticlechunk_hash
    ON largeparticlechunk(chunkhashhex);

-----------------------------------------------------------------------
-- 3. Integrity windows over chunks (Merkle subtrees, audit records)
-----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS largeparticleintegritywindow (
    fileid               TEXT NOT NULL REFERENCES largeparticlefile(fileid)
                              ON DELETE CASCADE,
    windowid             TEXT NOT NULL,              -- stable identifier
    windowstartchunk     INTEGER NOT NULL,           -- inclusive
    windowendchunk       INTEGER NOT NULL,           -- inclusive
    windowmerkleroothex  TEXT NOT NULL,              -- Merkle root over [start,end]
    filemerkleroothex    TEXT NOT NULL,              -- must match parent file
    createdutc           TEXT NOT NULL,
    integritygrade       TEXT NOT NULL,              -- A,B,C,D
    checkedbydid         TEXT,                       -- DID of verifier
    evidencehex          TEXT,
    PRIMARY KEY (fileid, windowid),
    CHECK (windowstartchunk >= 0),
    CHECK (windowendchunk >= windowstartchunk),
    CHECK (integritygrade IN ('A','B','C','D'))
);

CREATE INDEX IF NOT EXISTS idx_largeparticleintegritywindow_grade
    ON largeparticleintegritywindow(fileid, integritygrade);

-----------------------------------------------------------------------
-- 4. Policy table (default chunk size, bounds, hash algo)
-----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS largeparticlefilepolicy (
    policyid              TEXT PRIMARY KEY,
    versiontag            TEXT NOT NULL,
    defaultchunksizebytes INTEGER NOT NULL,
    minchunksizebytes     INTEGER NOT NULL,
    maxchunksizebytes     INTEGER NOT NULL,
    hashalgo              TEXT NOT NULL,
    maxfilesizebytes      INTEGER NOT NULL,
    comment               TEXT,
    CHECK (defaultchunksizebytes > 0),
    CHECK (minchunksizebytes > 0),
    CHECK (maxchunksizebytes >= minchunksizebytes),
    CHECK (maxfilesizebytes > 0),
    CHECK (hashalgo IN ('ORGANIC256'))
);

INSERT OR IGNORE INTO largeparticlefilepolicy (
    policyid,
    versiontag,
    defaultchunksizebytes,
    minchunksizebytes,
    maxchunksizebytes,
    hashalgo,
    maxfilesizebytes,
    comment
) VALUES (
    'default.largeparticlefile.2026v1',
    '2026v1',
    1048576,
    262144,
    4194304,
    'ORGANIC256',
    1099511627776,
    'Default large file policy, 1 MiB chunks, ORGANIC256 hashing.'
);

-----------------------------------------------------------------------
-- 5. Views for agents and CI
-----------------------------------------------------------------------

-- Summary view: one row per file with basic KER wiring surface.
CREATE VIEW IF NOT EXISTS vlargeparticlefile_summary AS
SELECT
    f.fileid,
    f.assetclass,
    f.logicalname,
    f.region,
    f.ecoscope,
    f.ownerbostrom,
    f.repotarget,
    f.relpath,
    f.totallengthbytes,
    f.chunksizebytes,
    f.chunkcount,
    f.filemerkleroothex,
    f.filehashalgo,
    f.contenttype,
    f.createdutc,
    f.updatedutc,
    f.lane,
    f.kerdeployable
FROM largeparticlefile AS f;

-- AI-chat friendly view: join file and chunk counts, filter by region.
CREATE VIEW IF NOT EXISTS vagent_largeparticlefile_phx AS
SELECT
    f.fileid,
    f.logicalname,
    f.assetclass,
    f.repotarget,
    f.relpath,
    f.totallengthbytes,
    f.chunksizebytes,
    f.chunkcount,
    f.filemerkleroothex,
    f.lane,
    f.kerdeployable
FROM largeparticlefile AS f
WHERE f.region = 'Phoenix-AZ';
