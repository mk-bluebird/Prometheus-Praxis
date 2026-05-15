-- filename: db_restoration_planes.sql
-- destination: eco_restoration_shard/db/db_restoration_planes.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS restoration_plane (
    planeid        TEXT PRIMARY KEY,  -- e.g. 'restoration_mass', 'restoration_karma'
    description    TEXT NOT NULL,
    weight         REAL NOT NULL CHECK (weight >= 0.0),
    nonoffsettable INTEGER NOT NULL DEFAULT 1 CHECK (nonoffsettable IN (0,1)),
    safeband       REAL,
    goldband       REAL,
    hardband       REAL,
    contractid     TEXT NOT NULL,     -- e.g. 'EcosafetyContinuity2026v1'
    createdutc     TEXT NOT NULL,
    updatedutc     TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS restoration_coordinate (
    coordinatekey  TEXT PRIMARY KEY,  -- e.g. 'restoration_radius', 'restoration_mass_gain'
    planeid        TEXT NOT NULL,
    normalization  TEXT NOT NULL,     -- human-readable normalization rule
    units          TEXT NOT NULL,     -- e.g. 'm', 'kg', 'dimensionless'
    corridor_ref   TEXT NOT NULL,     -- pointer to corridor definition key
    createdutc     TEXT NOT NULL,
    updatedutc     TEXT NOT NULL,
    FOREIGN KEY (planeid) REFERENCES restoration_plane(planeid) ON DELETE CASCADE
);
