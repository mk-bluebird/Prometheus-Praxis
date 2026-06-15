-- filename: dbcyboquatic_ecomachinery.sql
-- destination: eco_restoration_shard/sql/spine/dbcyboquatic_ecomachinery.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Core catalog of Cyboquatic machines and nodes
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_machine (
    machine_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    machinename     TEXT NOT NULL,              -- e.g. FLOWVAC-CHANNEL-01
    machinetype     TEXT NOT NULL,              -- e.g. FLOWVAC, MAR-VAULT, TRAYLINE
    nodeid          TEXT NOT NULL,              -- logical node in EcoNet spine
    region          TEXT NOT NULL,              -- e.g. Phoenix-AZ
    medium          TEXT NOT NULL,              -- water, soil, air, bio
    lane            TEXT NOT NULL,              -- RESEARCH, EXPPROD, PROD
    substrate_code  TEXT,                       -- biodegradable substrate family
    design_spechash TEXT NOT NULL,              -- ALNSPECHASHHEX of design spec
    created_utc     TEXT NOT NULL DEFAULT strftime('%Y-%m-%dT%H:%M:%SZ','now'),
    updated_utc     TEXT NOT NULL DEFAULT strftime('%Y-%m-%dT%H:%M:%SZ','now')
);

CREATE TRIGGER IF NOT EXISTS trg_cybo_machine_updated
AFTER UPDATE ON cybo_machine
BEGIN
    UPDATE cybo_machine
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE machine_id = NEW.machine_id;
END;

CREATE INDEX IF NOT EXISTS idx_cybo_machine_node
    ON cybo_machine(nodeid, region, machinetype);

----------------------------------------------------------------------
-- 2. Eco-metric windows per machine (KER + eco energy/materials/bio)
--    Non-actuating, diagnostic only.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_machine_window (
    window_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    machine_id      INTEGER NOT NULL REFERENCES cybo_machine(machine_id)
                       ON DELETE CASCADE,
    shardid         INTEGER,                    -- FK into shardinstance.shardid when present
    window_start    TEXT NOT NULL,              -- ISO8601
    window_end      TEXT NOT NULL,
    kmetric         REAL NOT NULL CHECK (kmetric >= 0.0 AND kmetric <= 1.0),
    emetric         REAL NOT NULL CHECK (emetric >= 0.0 AND emetric <= 1.0),
    rmetric         REAL NOT NULL CHECK (rmetric >= 0.0 AND rmetric <= 1.0),
    vt              REAL NOT NULL CHECK (vt >= 0.0),
    rcarbon         REAL,                       -- 0..1 carbon risk coord
    rbiodiv         REAL,                       -- 0..1 biodiversity risk coord
    rmaterials      REAL,                       -- 0..1 materials toxicity / persistence
    rdataquality    REAL,                       -- 0..1, from rcalib/rsigma
    eco_gain        REAL NOT NULL,              -- dimensionless eco gain
    energy_kwh      REAL NOT NULL,              -- energy consumption for window
    material_masskg REAL NOT NULL,              -- mass processed / deployed
    biodeg_fraction REAL,                       -- 0..1 fraction degraded in window
    non_toxicity    REAL,                       -- 0..1 lower is worse
    evidencehex     TEXT NOT NULL,              -- provenance kernel hex
    signinghex      TEXT NOT NULL,              -- DID-bound signature hex
    created_utc     TEXT NOT NULL DEFAULT strftime('%Y-%m-%dT%H:%M:%SZ','now')
);

CREATE INDEX IF NOT EXISTS idx_cybo_machine_window_machine
    ON cybo_machine_window(machine_id, window_start, window_end);

CREATE INDEX IF NOT EXISTS idx_cybo_machine_window_ker
    ON cybo_machine_window(kmetric, emetric, rmetric);

----------------------------------------------------------------------
-- 3. Energy-per-eco-gain diagnostic view (eco-per-joule)
--    This is T02ECOPERJOULEROUTER-aligned, read-only.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_cybo_machine_ecoperjoule AS
SELECT
    m.machine_id,
    m.machinename,
    m.machinetype,
    m.nodeid,
    m.region,
    m.medium,
    m.lane,
    w.window_start,
    w.window_end,
    w.kmetric,
    w.emetric,
    w.rmetric,
    w.eco_gain,
    w.energy_kwh,
    CASE
        WHEN w.energy_kwh > 0.0 THEN w.eco_gain / w.energy_kwh
        ELSE NULL
    END AS eco_per_kwh,
    w.material_masskg,
    w.rcarbon,
    w.rbiodiv,
    w.rmaterials,
    w.rdataquality,
    w.vt
FROM cybo_machine AS m
JOIN cybo_machine_window AS w
  ON w.machine_id = m.machine_id;

----------------------------------------------------------------------
-- 4. Blast-radius linkage shim into existing blastradiuslink
--    Uses the existing blastradiuslink table seeded in the EcoNet spine.
----------------------------------------------------------------------

-- This view joins cybo machines to existing blastradiuslink evidence.
-- It assumes blastradiuslink already exists as in econetindexsrcmigrationcyboquaticblastradiusspine.rs.
-- If blastradiuslink is missing, migrations will fail and must be fixed in EcoNet spine.

CREATE VIEW IF NOT EXISTS v_cybo_machine_blastradius AS
SELECT
    m.machine_id,
    m.machinename,
    m.nodeid,
    m.region,
    br.impacttype,
    br.impactscore,
    br.vtsensitivity,
    br.notes
FROM cybo_machine AS m
JOIN blastradiuslink AS br
  ON br.sourcetype = 'SHARD'
 AND br.targettype = 'NODE'
 AND br.targetid  = m.nodeid;

----------------------------------------------------------------------
-- 5. Simple eco-risk score for ranking (non-actuating).
--    Combines K,E,R with non-offsettable planes rcarbon, rbiodiv, rmaterials.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_cybo_machine_ecorank AS
SELECT
    m.machine_id,
    m.machinename,
    m.machinetype,
    m.nodeid,
    m.region,
    m.lane,
    AVG(w.kmetric) AS kavg,
    AVG(w.emetric) AS eavg,
    AVG(w.rmetric) AS ravg,
    AVG(COALESCE(w.rcarbon,    0.0)) AS rcarbon_avg,
    AVG(COALESCE(w.rbiodiv,    0.0)) AS rbiodiv_avg,
    AVG(COALESCE(w.rmaterials, 0.0)) AS rmaterials_avg,
    AVG(COALESCE(w.rdataquality, 0.0)) AS rdataquality_avg,
    AVG(w.vt) AS vtavg
FROM cybo_machine AS m
JOIN cybo_machine_window AS w
  ON w.machine_id = m.machine_id
GROUP BY
    m.machine_id,
    m.machinename,
    m.machinetype,
    m.nodeid,
    m.region,
    m.lane;
