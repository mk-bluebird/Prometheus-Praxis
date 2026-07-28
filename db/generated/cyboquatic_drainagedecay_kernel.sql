-- filename: db/generated/cyboquatic_drainagedecay_kernel.sql
-- license: MIT OR Apache-2.0
-- generated from ALN particle cyboquatic.drainagedecay.kernel
CREATE TABLE IF NOT EXISTS cyboquatic_drainagedecay_kernel (
    frameId TEXT NOT NULL,
    canalNodeId TEXT NOT NULL,
    kerProfileId TEXT NOT NULL,
    timestampUtc TEXT NOT NULL,
    bodMgL REAL NOT NULL,
    tssMgL REAL NOT NULL,
    cecCmolPerKg REAL NOT NULL,
    frameEnergyJ REAL NOT NULL,
    deltaVtMps REAL NOT NULL,
    k REAL NOT NULL,
    e REAL NOT NULL,
    r REAL NOT NULL,
    kerScore REAL NOT NULL,
    fogRegionId TEXT NOT NULL,
    fogChannelId TEXT NOT NULL,
    governanceParticleHex TEXT NOT NULL,
    evidenceHex TEXT NOT NULL,
    signingDid TEXT NOT NULL
);
