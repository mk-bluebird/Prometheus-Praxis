-- filename db_planeweights_schema.sql
-- destination Eco-Fort/db/db_planeweights_schema.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS planeweights (
    planeweightsid  INTEGER PRIMARY KEY AUTOINCREMENT,
    contractid      TEXT NOT NULL,
    planeid         TEXT NOT NULL,
    weight          REAL NOT NULL CHECK (weight >= 0.0),
    nonoffsettable  INTEGER NOT NULL CHECK (nonoffsettable IN (0,1)),
    softband        REAL NOT NULL CHECK (softband >= 0.0 AND softband <= 1.0),
    hardband        REAL NOT NULL CHECK (hardband >= 0.0 AND hardband <= 1.0),
    uncertaintycap  REAL NOT NULL CHECK (uncertaintycap >= 0.0 AND uncertaintycap <= 1.0),
    versiontag      TEXT NOT NULL,
    proofrefhex     TEXT NOT NULL,
    created_utc     TEXT NOT NULL DEFAULT (datetime('now')),
    updated_utc     TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE (contractid, planeid, versiontag)
);
