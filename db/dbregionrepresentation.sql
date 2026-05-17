-- filename db/dbregionrepresentation.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS region_representation (
    regionid                TEXT PRIMARY KEY,
    sensordensityindex      REAL NOT NULL, -- 0..1
    representation_floor    REAL NOT NULL, -- min fractional share, 0..1
    max_trust_weight        REAL NOT NULL, -- cap on trust multiplier
    last_updated_utc        TEXT NOT NULL,
    evidencehex             TEXT NOT NULL
);
