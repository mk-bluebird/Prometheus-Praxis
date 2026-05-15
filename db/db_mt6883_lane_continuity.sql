-- filename: db_mt6883_lane_continuity.sql
-- destination: eco_restoration_shard/db/db_mt6883_lane_continuity.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 1. Extend lanestatusshard with mt6883 continuity and neuroethic flags

ALTER TABLE lanestatusshard
    ADD COLUMN mt6883_registry_id INTEGER;

ALTER TABLE lanestatusshard
    ADD COLUMN mt6883_ok INTEGER DEFAULT 0 CHECK (mt6883_ok IN (0, 1));

ALTER TABLE lanestatusshard
    ADD COLUMN neuroethic_radius_hours REAL;

ALTER TABLE lanestatusshard
    ADD COLUMN neuroethic_ok INTEGER DEFAULT 0 CHECK (neuroethic_ok IN (0, 1));

ALTER TABLE lanestatusshard
    ADD COLUMN author_bostrom TEXT;

ALTER TABLE lanestatusshard
    ADD COLUMN author_contractid TEXT;

ALTER TABLE lanestatusshard
    ADD COLUMN author_comment TEXT;

CREATE INDEX IF NOT EXISTS idx_lanestatus_mt6883
ON lanestatusshard (
    region,
    lane,
    mt6883_ok,
    neuroethic_ok
);

-- 2. Extend blastradiusindex with neuroethic-aware radius for MT6883

ALTER TABLE blastradiusindex
    ADD COLUMN neuroethic_radius_hours REAL;

ALTER TABLE blastradiusindex
    ADD COLUMN neuroethic_notes TEXT;

CREATE INDEX IF NOT EXISTS idx_blastradius_mt6883_neuro
ON blastradiusindex (
    region,
    planeid,
    neuroethic_radius_hours
);

-- 3. MT6883 continuity governance view

DROP VIEW IF EXISTS v_mt6883_lane_continuity;

CREATE VIEW v_mt6883_lane_continuity AS
SELECT
    l.kernelid,
    l.region,
    l.lane,
    l.kscore,
    l.escore,
    l.rscore,
    l.vtmax,
    l.planesok,
    l.topologyok,
    l.mt6883_registry_id,
    l.mt6883_ok,
    l.neuroethic_radius_hours,
    l.neuroethic_ok,
    l.author_bostrom,
    l.author_contractid,
    l.author_comment,
    l.createdutc
FROM lanestatusshard AS l
WHERE l.mt6883_registry_id IS NOT NULL;

-- 4. MT6883 blast radius neuroethic view (non-actuating)

DROP VIEW IF EXISTS v_blastradius_mt6883_neuro;

CREATE VIEW v_blastradius_mt6883_neuro AS
SELECT
    b.scoperef          AS nodeid,
    b.region,
    b.planeid,
    b.graphid,
    b.radiusm,
    b.radiustimes,
    b.neuroethic_radius_hours,
    b.kerband,
    b.topologygrade,
    b.nonactuating,
    b.neuroethic_notes,
    b.author_bostrom,
    b.author_contractid,
    b.author_comment,
    b.createdutc
FROM blastradiusindex AS b
WHERE b.neuroethic_radius_hours IS NOT NULL;
