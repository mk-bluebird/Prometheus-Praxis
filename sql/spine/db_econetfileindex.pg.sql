-- filename: sql/spine/db_econetfileindex.pg.sql
-- destination: ecorestoration_shard/sql/spine/db_econetfileindex.pg.sql

CREATE SCHEMA IF NOT EXISTS eco_idx;

CREATE TABLE IF NOT EXISTS eco_idx.econetfileindex (
    filename        TEXT        NOT NULL,
    destination     TEXT        NOT NULL,
    repotarget      TEXT        NOT NULL,
    roleband        TEXT        NOT NULL CHECK (roleband IN ('SPINE','RESEARCH','ENGINE','MATERIAL','GOV','APP')),
    lanedefault     TEXT        NOT NULL CHECK (lanedefault IN ('RESEARCH','EXPPROD','PROD')),
    regionscope     TEXT        NOT NULL,
    planes          TEXT,
    logicalname     TEXT        NOT NULL,
    artifactkind    TEXT        NOT NULL,
    econscope       TEXT        NOT NULL,
    contractid      TEXT,
    nonactuating    INTEGER     NOT NULL CHECK (nonactuating IN (0,1)),
    kerbandk        REAL        CHECK (kerbandk >= 0.0 AND kerbandk <= 1.0),
    kerbande        REAL        CHECK (kerbande >= 0.0 AND kerbande <= 1.0),
    kerbandr        REAL        CHECK (kerbandr >= 0.0 AND kerbandr <= 1.0),
    authorbostrom   TEXT        NOT NULL,
    ownerlabel      TEXT        CHECK (ownerlabel IN ('PRIMARY','ALTERNATE','SAFEALT')),
    evidencehex     TEXT,
    dbshardlogical  TEXT,
    dbrole          TEXT        CHECK (dbrole IN ('GOVERNANCE','TELEMETRY','INDEX')),
    tasklogical     TEXT,
    status          TEXT        NOT NULL CHECK (status IN ('DRAFT','ACTIVE','DEPRECATED')),
    createdutc      TEXT        NOT NULL,
    updatedutc      TEXT        NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_econetfileindex_agent
    ON eco_idx.econetfileindex (repotarget, regionscope, econscope, lanedefault, roleband, artifactkind);

CREATE INDEX IF NOT EXISTS idx_econetfileindex_kerband
    ON eco_idx.econetfileindex (regionscope, lanedefault, kerbandk, kerbande, kerbandr);

CREATE INDEX IF NOT EXISTS idx_econetfileindex_author
    ON eco_idx.econetfileindex (authorbostrom, regionscope, dbrole, status);
