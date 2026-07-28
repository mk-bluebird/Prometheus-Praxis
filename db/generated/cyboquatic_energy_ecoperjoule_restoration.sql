-- filename: db/generated/cyboquatic_energy_ecoperjoule_restoration.sql
-- license: MIT OR Apache-2.0
-- generated from ALN particle cyboquatic.energy.ecoperjoule.restoration
CREATE TABLE IF NOT EXISTS cyboquatic_energy_ecoperjoule_restoration (
    frameId TEXT NOT NULL,
    nodeId TEXT NOT NULL,
    dateUtc TEXT NOT NULL,
    windowStartUtc TEXT NOT NULL,
    windowEndUtc TEXT NOT NULL,
    energyReqJ REAL NOT NULL,
    ecoperJoule REAL NOT NULL,
    restorationFlag INTEGER NOT NULL CHECK (restorationFlag IN (0, 1)),
    carbonNegativeOk INTEGER NOT NULL CHECK (carbonNegativeOk IN (0, 1)),
    k REAL NOT NULL,
    e REAL NOT NULL,
    r REAL NOT NULL,
    kerScore REAL NOT NULL,
    evidenceHex TEXT NOT NULL,
    signingDid TEXT NOT NULL
);
