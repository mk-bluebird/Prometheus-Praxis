-- filename: db/db_android_index.sql
-- destination: eco_restoration_shard/db/db_android_index.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS androidtoolindex (
    toolid INTEGER PRIMARY KEY AUTOINCREMENT,
    packagename TEXT NOT NULL,
    activityname TEXT NOT NULL,
    helpername TEXT NOT NULL,
    purpose TEXT NOT NULL,
    dbfile_econet TEXT NOT NULL,
    dbfile_restoration TEXT NOT NULL,
    region TEXT NOT NULL,
    scope TEXT NOT NULL,
    lane_band TEXT NOT NULL,
    nonactuating INTEGER NOT NULL DEFAULT 1 CHECK (nonactuating IN (0,1)),
    author_bostrom TEXT NOT NULL,
    createdutc TEXT NOT NULL,
    updatedutc TEXT NOT NULL,
    UNIQUE (packagename, activityname)
);

INSERT OR IGNORE INTO androidtoolindex (
    packagename,
    activityname,
    helpername,
    purpose,
    dbfile_econet,
    dbfile_restoration,
    region,
    scope,
    lane_band,
    nonactuating,
    author_bostrom,
    createdutc,
    updatedutc
) VALUES (
    'org.econet.blast',
    'BlastRadiusKerInspectorActivity',
    'KerInspectorDbHelper',
    'Read-only Android inspector for blast radius, KER, eco-per-joule, and MT6883 continuity',
    'econetconstellationindex.db',
    'restorationindex.sqlite3',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCE',
    1,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now'),
    datetime('now')
);
