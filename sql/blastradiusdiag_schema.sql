-- filename: sql/blastradiusdiag_schema.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS blastradiusdiag (
    diagid            TEXT PRIMARY KEY,
    nodeid            TEXT NOT NULL,
    fogregionid       TEXT NOT NULL,
    timestamputc      TEXT NOT NULL,
    rhydraulic        REAL NOT NULL,
    rbio              REAL NOT NULL,
    rtox              REAL NOT NULL,
    vtbefore          REAL NOT NULL,
    vtafter           REAL NOT NULL,
    deltavt           REAL NOT NULL,
    kfactor           REAL NOT NULL,
    efactor           REAL NOT NULL,
    rfactor           REAL NOT NULL,
    kerscore          REAL NOT NULL,
    lane              TEXT NOT NULL,
    aln_hexstamp      TEXT,
    CHECK (rhydraulic BETWEEN 0.0 AND 1.0),
    CHECK (rbio       BETWEEN 0.0 AND 1.0),
    CHECK (rtox       BETWEEN 0.0 AND 1.0),
    CHECK (kfactor    BETWEEN 0.0 AND 1.0),
    CHECK (efactor    BETWEEN 0.0 AND 1.0),
    CHECK (rfactor    BETWEEN 0.0 AND 1.0),
    CHECK (kerscore   BETWEEN 0.0 AND 1.0),
    CHECK (vtafter    >= 0.0)
);

CREATE INDEX IF NOT EXISTS idx_blastradiusdiag_node_time
    ON blastradiusdiag (nodeid, timestamputc);

CREATE INDEX IF NOT EXISTS idx_blastradiusdiag_fogregion_time
    ON blastradiusdiag (fogregionid, timestamputc);
