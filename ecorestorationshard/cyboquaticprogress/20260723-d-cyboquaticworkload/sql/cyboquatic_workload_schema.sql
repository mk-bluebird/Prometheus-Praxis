-- ecorestorationshard/cyboquaticprogress/20260723-d-cyboquaticworkload/sql/cyboquatic_workload_schema.sql
-- Cyboquatic workload telemetry schema (energyreqJ, ΔVt, KER, residual).
-- Portable SQLite-focused DDL with KER and Lyapunov invariants.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS cybo_workload_node (
    nodeid TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    latitudedeg REAL NOT NULL,
    longitudedeg REAL NOT NULL,
    fogregionid TEXT NOT NULL,
    fogchannelid TEXT NOT NULL,
    energycorridormaxj REAL NOT NULL CHECK (energycorridormaxj > 0.0),
    hydrauliccorridormax REAL NOT NULL CHECK (hydrauliccorridormax > 0.0),
    carboncorridormax REAL NOT NULL CHECK (carboncorridormax > 0.0),
    createdatutc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS cybo_workload_frame (
    frameid TEXT PRIMARY KEY,
    nodeid TEXT NOT NULL,
    timestamputc TEXT NOT NULL,
    energyreqj REAL NOT NULL CHECK (energyreqj >= 0.0),
    energycorridormaxj REAL NOT NULL CHECK (energycorridormaxj > 0.0),
    hydraulicload REAL NOT NULL CHECK (hydraulicload >= 0.0),
    hydrauliccorridormax REAL NOT NULL CHECK (hydrauliccorridormax > 0.0),
    carbonintensity REAL NOT NULL CHECK (carbonintensity >= 0.0),
    carboncorridormax REAL NOT NULL CHECK (carboncorridormax > 0.0),
    uncertaintyraw REAL NOT NULL CHECK (uncertaintyraw >= 0.0 AND uncertaintyraw <= 1.0),
    renergy REAL NOT NULL CHECK (renergy >= 0.0 AND renergy <= 1.0),
    rhydraulics REAL NOT NULL CHECK (rhydraulics >= 0.0 AND rhydraulics <= 1.0),
    rcarbon REAL NOT NULL CHECK (rcarbon >= 0.0 AND rcarbon <= 1.0),
    runcertainty REAL NOT NULL CHECK (runcertainty >= 0.0 AND runcertainty <= 1.0),
    vtbefore REAL NOT NULL CHECK (vtbefore >= 0.0),
    vtafter REAL NOT NULL CHECK (vtafter >= 0.0),
    deltavt REAL NOT NULL,
    k REAL NOT NULL CHECK (k >= 0.0 AND k <= 1.0),
    e REAL NOT NULL CHECK (e >= 0.0 AND e <= 1.0),
    r REAL NOT NULL CHECK (r >= 0.0 AND r <= 1.0),
    kerscore REAL NOT NULL CHECK (kerscore > 0.0),
    fogregionid TEXT NOT NULL,
    fogchannelid TEXT NOT NULL,
    governanceparticlehex TEXT NOT NULL,
    FOREIGN KEY (nodeid) REFERENCES cybo_workload_node(nodeid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_workload_node_region
    ON cybo_workload_node(fogregionid, fogchannelid);

CREATE INDEX IF NOT EXISTS idx_workload_frame_node_time
    ON cybo_workload_frame(nodeid, timestamputc);

CREATE INDEX IF NOT EXISTS idx_workload_frame_kerscore
    ON cybo_workload_frame(kerscore);

DROP TRIGGER IF EXISTS trg_workload_ker_invariant;
CREATE TRIGGER trg_workload_ker_invariant
BEFORE INSERT ON cybo_workload_frame
BEGIN
    SELECT
        CASE
            WHEN (NEW.k * NEW.e - NEW.r) <= 0.0 THEN
                RAISE(ABORT, 'KER score must be positive (k*e - r > 0)')
        END;

    SELECT
        CASE
            WHEN ABS(NEW.kerscore - (NEW.k * NEW.e - NEW.r)) > 0.000001 THEN
                RAISE(ABORT, 'kerscore inconsistent with KER triad')
        END;

    SELECT
        CASE
            WHEN NEW.deltavt > 0.0 THEN
                RAISE(ABORT, 'deltaVt must be <= 0 for non-regressive workload')
        END;
END;
