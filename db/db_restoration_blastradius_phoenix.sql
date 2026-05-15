-- filename: db_restoration_blastradius_phoenix.sql
-- destination: eco_restoration_shard/db/db_restoration_blastradius_phoenix.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 1. Extend blastradiusindex with restoration-aware fields (idempotent pattern)

ALTER TABLE blastradiusindex
    ADD COLUMN restorationradius_m REAL;

ALTER TABLE blastradiusindex
    ADD COLUMN restorationradius_hours REAL;

ALTER TABLE blastradiusindex
    ADD COLUMN deltamass_window_kg REAL;

ALTER TABLE blastradiusindex
    ADD COLUMN deltakarma_window REAL;

ALTER TABLE blastradiusindex
    ADD COLUMN restoration_ok INTEGER DEFAULT 0 CHECK (restoration_ok IN (0, 1));

ALTER TABLE blastradiusindex
    ADD COLUMN gw_risk_max REAL;

ALTER TABLE blastradiusindex
    ADD COLUMN author_bostrom TEXT;

ALTER TABLE blastradiusindex
    ADD COLUMN author_contractid TEXT;

ALTER TABLE blastradiusindex
    ADD COLUMN author_comment TEXT;

-- 2. Helper index for Phoenix restoration queries

CREATE INDEX IF NOT EXISTS idx_blastradius_restoration_phx
ON blastradiusindex (
    region,
    planeid,
    restoration_ok,
    restorationradius_m
);

-- 3. Phoenix restoration view (non-actuating, governance-only)

DROP VIEW IF EXISTS v_blastradius_restoration_phx;

CREATE VIEW v_blastradius_restoration_phx AS
SELECT
    b.scoperef            AS nodeid,
    b.region,
    b.planeid,
    b.graphid,
    b.radiusm             AS impact_radius_m,
    b.radiustimes         AS impact_radius_hours,
    b.restorationradius_m,
    b.restorationradius_hours,
    b.deltamass_window_kg,
    b.deltakarma_window,
    b.gw_risk_max,
    b.restoration_ok,
    b.kerband,
    b.topologygrade,
    b.nonactuating,
    b.author_bostrom,
    b.author_contractid,
    b.author_comment,
    b.createdutc
FROM blastradiusindex AS b
WHERE b.region = 'Phoenix-AZ'
  AND b.restorationradius_m IS NOT NULL;

-- 4. Phoenix restoration nodes view (strict gating for governance and CI)

DROP VIEW IF EXISTS v_restoration_nodes_phx;

CREATE VIEW v_restoration_nodes_phx AS
SELECT
    nodeid,
    region,
    planeid,
    graphid,
    restorationradius_m,
    restorationradius_hours,
    deltamass_window_kg,
    deltakarma_window,
    gw_risk_max,
    kerband,
    topologygrade,
    nonactuating,
    author_bostrom,
    author_contractid,
    author_comment,
    createdutc
FROM v_blastradius_restoration_phx
WHERE restoration_ok = 1
  AND deltamass_window_kg >= 0.0
  AND deltakarma_window  >= 0.0
  AND gw_risk_max        <= 1.0;
