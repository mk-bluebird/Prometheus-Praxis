-- filename db_lane_status_shard.sql
-- destination Eco-Fort/db/db_lane_status_shard.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS lane_status_shard (
    lanestatusid     INTEGER PRIMARY KEY AUTOINCREMENT,
    shardid          INTEGER NOT NULL REFERENCES shardinstance(shardid) ON DELETE CASCADE,
    repo_id          INTEGER NOT NULL REFERENCES repo(repoid) ON DELETE CASCADE,
    author_login     TEXT NOT NULL,
    author_binding_id INTEGER,
    lane_from        TEXT NOT NULL CHECK (lane_from IN ('RESEARCH', 'EXPPROD', 'PROD')),
    lane_to          TEXT NOT NULL CHECK (lane_to IN ('RESEARCH', 'EXPPROD', 'PROD')),
    promotion_kind   TEXT NOT NULL CHECK (promotion_kind IN ('PROMOTION', 'DEMOTION')),
    decision_did     TEXT NOT NULL,
    decision_utc     TEXT NOT NULL,
    evidencehex      TEXT NOT NULL,
    nonactuatingonly INTEGER NOT NULL CHECK (nonactuatingonly IN (0,1))
);

INSERT INTO lane_status_shard (
    shardid,
    repo_id,
    author_login,
    author_binding_id,
    lane_from,
    lane_to,
    promotion_kind,
    decision_did,
    decision_utc,
    evidencehex,
    nonactuatingonly
)
VALUES (
    1234,
    (SELECT repoid FROM repo WHERE githubslug = 'mk-bluebird/eco_restoration_shard'),
    'mk-bluebird',
    (SELECT binding_id
     FROM eco_repo_identity_binding
     WHERE repo_id = (SELECT repoid FROM repo WHERE githubslug = 'mk-bluebird/eco_restoration_shard')
       AND github_login = 'mk-bluebird'
       AND is_current_owner = 1
     LIMIT 1),
    'RESEARCH',
    'EXPPROD',
    'PROMOTION',
    'did:bostrom:bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    '2026-05-14T00:00:00Z',
    'ab12cd34ef56',
    1
);
