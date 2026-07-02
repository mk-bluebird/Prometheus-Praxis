-- Econet repo index entry for eco_restoration_shard, RESEARCH, non-actuating.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS econetrepoindex (
    reponame        TEXT PRIMARY KEY,
    githubslug      TEXT NOT NULL,
    roleband        TEXT NOT NULL CHECK (roleband IN ('RESEARCH','EXPPROD','PROD')),
    primaryplane    TEXT NOT NULL,
    nonactuatingonly INTEGER NOT NULL CHECK (nonactuatingonly IN (0,1)),
    region          TEXT NOT NULL,
    didowner        TEXT NOT NULL,
    createdutc      TEXT NOT NULL,
    updatedutc      TEXT NOT NULL
);

INSERT OR REPLACE INTO econetrepoindex (
    reponame,
    githubslug,
    roleband,
    primaryplane,
    nonactuatingonly,
    region,
    didowner,
    createdutc,
    updatedutc
)
VALUES (
    'eco_restoration_shard',
    'mk-bluebird/eco_restoration_shard',
    'RESEARCH',
    'CARBON',
    1,
    'Phoenix-AZ',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now'),
    datetime('now')
);
