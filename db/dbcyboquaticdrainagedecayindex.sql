-- filename: db/dbcyboquaticdrainagedecayindex.sql
-- license: MIT OR Apache-2.0
-- role: Long-lived cyboquatic drainagedecay index for BOD/TSS/CEC frames.
-- note: Aligns with canalnode, kerprofile, and drainagedecayframe schemas from 2026‑07‑22 shard.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Canonical canal node and KER profile tables (if not already present)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS canalnode (
    canalnodeid        TEXT PRIMARY KEY,
    name               TEXT NOT NULL,
    latitudedeg        REAL NOT NULL,
    longitudedeg       REAL NOT NULL,
    fogregionid        TEXT NOT NULL,
    fogchannelid       TEXT NOT NULL,
    maxframeenergyj    REAL NOT NULL CHECK (maxframeenergyj >= 0.0),
    maxbodmgl          REAL NOT NULL CHECK (maxbodmgl       >= 0.0),
    maxtssmgl          REAL NOT NULL CHECK (maxtssmgl       >= 0.0),
    maxceccmolperkg    REAL NOT NULL CHECK (maxceccmolperkg >= 0.0),
    createdatutc       TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS kerprofile (
    kerprofileid       TEXT PRIMARY KEY,
    canalnodeid        TEXT NOT NULL,
    kknowledgefactor   REAL NOT NULL CHECK (kknowledgefactor >= 0.0 AND kknowledgefactor <= 1.0),
    eecoimpact         REAL NOT NULL CHECK (eecoimpact      >= 0.0 AND eecoimpact      <= 1.0),
    rriskfactor        REAL NOT NULL CHECK (rriskfactor     >= 0.0 AND rriskfactor     <= 1.0),
    kerscore           REAL NOT NULL,
    governanceparticlehex TEXT NOT NULL,
    createdatutc       TEXT NOT NULL,
    FOREIGN KEY (canalnodeid) REFERENCES canalnode (canalnodeid) ON DELETE CASCADE
);

----------------------------------------------------------------------
-- 2. Drainage-decay index table
-- Generalized version of the 2026‑07‑22 drainagedecayframe schema.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cyboquatic_drainagedecay_index (
    frameid            TEXT PRIMARY KEY,
    canalnodeid        TEXT NOT NULL,
    kerprofileid       TEXT NOT NULL,

    timestamputc       TEXT NOT NULL,

    bodmgl             REAL NOT NULL CHECK (bodmgl  >= 0.0 AND bodmgl  <= 80.0),
    tssmgl             REAL NOT NULL CHECK (tssmgl  >= 0.0 AND tssmgl  <= 500.0),
    ceccmolperkg       REAL NOT NULL CHECK (ceccmolperkg >= 0.0 AND ceccmolperkg <= 100.0),

    frameenergyj       REAL NOT NULL CHECK (frameenergyj >= 0.0),
    deltavtmps         REAL NOT NULL CHECK (deltavtmps  >= -5.0 AND deltavtmps <= 5.0),

    -- Normalized KER coordinates
    kknowledgefactor   REAL NOT NULL CHECK (kknowledgefactor >= 0.0 AND kknowledgefactor <= 1.0),
    eecoimpact         REAL NOT NULL CHECK (eecoimpact      >= 0.0 AND eecoimpact      <= 1.0),
    rriskfactor        REAL NOT NULL CHECK (rriskfactor     >= 0.0 AND rriskfactor     <= 1.0),
    kerscore           REAL NOT NULL,

    -- FOG and governance bindings
    fogregionid        TEXT NOT NULL,
    fogchannelid       TEXT NOT NULL,
    governanceparticlehex TEXT NOT NULL,
    evidence_hex       TEXT NOT NULL,
    signing_did        TEXT NOT NULL,

    created_at_utc     TEXT NOT NULL,
    last_updated_utc   TEXT NOT NULL,

    FOREIGN KEY (canalnodeid)  REFERENCES canalnode   (canalnodeid)   ON DELETE CASCADE,
    FOREIGN KEY (kerprofileid) REFERENCES kerprofile  (kerprofileid)  ON DELETE CASCADE
);

----------------------------------------------------------------------
-- 3. Indices
----------------------------------------------------------------------

CREATE INDEX IF NOT EXISTS idx_drainagedecay_node_time
    ON cyboquatic_drainagedecay_index (canalnodeid, timestamputc);

CREATE INDEX IF NOT EXISTS idx_drainagedecay_ker
    ON cyboquatic_drainagedecay_index (kknowledgefactor, eecoimpact, rriskfactor, kerscore);

CREATE INDEX IF NOT EXISTS idx_drainagedecay_fog
    ON cyboquatic_drainagedecay_index (fogregionid, fogchannelid);

CREATE INDEX IF NOT EXISTS idx_drainagedecay_evidence
    ON cyboquatic_drainagedecay_index (evidence_hex, signing_did);

----------------------------------------------------------------------
-- 4. KER invariant trigger
----------------------------------------------------------------------

DROP TRIGGER IF EXISTS trg_drainagedecay_ker_invariant;

CREATE TRIGGER trg_drainagedecay_ker_invariant
BEFORE INSERT ON cyboquatic_drainagedecay_index
BEGIN
    -- KER score must be positive and consistent with k * e - r.
    SELECT
        CASE
            WHEN NEW.kerscore <= 0.0
            THEN RAISE(ABORT, 'kerscore must be positive for drainagedecay')
        END;

    SELECT
        CASE
            WHEN ABS(NEW.kknowledgefactor * NEW.eecoimpact - NEW.rriskfactor - NEW.kerscore) > 0.000001
            THEN RAISE(ABORT, 'kerscore inconsistent with KER triad for drainagedecay')
        END;
END;
