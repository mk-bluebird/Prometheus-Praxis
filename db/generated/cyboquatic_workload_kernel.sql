-- filename: db/generated/cyboquatic_workload_kernel.sql
-- license: MIT OR Apache-2.0
-- generated from ALN particle cyboquatic.workload.kernel
CREATE TABLE IF NOT EXISTS cyboquatic_workload_kernel (
    nodeId TEXT NOT NULL,
    dateUtc TEXT NOT NULL,
    windowStartUtc TEXT NOT NULL,
    windowEndUtc TEXT NOT NULL,
    domainCode TEXT NOT NULL,
    subtaskId TEXT NOT NULL,
    energyReqJ REAL NOT NULL,
    throughputM3 REAL NOT NULL,
    headM REAL NOT NULL,
    dutyCycle REAL NOT NULL,
    vtCurrent REAL NOT NULL,
    vtNext REAL NOT NULL,
    vtDelta REAL NOT NULL,
    k REAL NOT NULL,
    e REAL NOT NULL,
    r REAL NOT NULL,
    kerScore REAL NOT NULL,
    lane TEXT NOT NULL,
    safeToPromote INTEGER NOT NULL CHECK (safeToPromote IN (0, 1)),
    evidenceHex TEXT NOT NULL,
    signingDid TEXT NOT NULL
);
