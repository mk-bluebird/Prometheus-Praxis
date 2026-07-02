-- Cyboquatic plane weights shard: mark Cyboquatic planes non-offsettable.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS planeweights (
    planeid          TEXT PRIMARY KEY,
    planename        TEXT NOT NULL,
    domain           TEXT NOT NULL,
    weight           REAL NOT NULL,
    nonoffsettable   INTEGER NOT NULL CHECK (nonoffsettable IN (0,1)),
    createdutc       TEXT NOT NULL,
    updatedutc       TEXT NOT NULL
);

INSERT OR REPLACE INTO planeweights (
    planeid,
    planename,
    domain,
    weight,
    nonoffsettable,
    createdutc,
    updatedutc
)
VALUES
    (
        'CyboquaticSurfaceCarbon',
        'Cyboquatic Surface Carbon',
        'CARBON',
        0.30,
        1,
        datetime('now'),
        datetime('now')
    ),
    (
        'CyboquaticHydrologyImpact',
        'Cyboquatic Hydrology Impact',
        'HYDRO',
        0.25,
        1,
        datetime('now'),
        datetime('now')
    ),
    (
        'CyboquaticRestorationRadius',
        'Cyboquatic Restoration Radius',
        'RESTORATION',
        0.25,
        1,
        datetime('now'),
        datetime('now')
    ),
    (
        'CyboquaticBlastAdjacency',
        'Cyboquatic Blast Adjacency',
        'BLAST',
        0.20,
        1,
        datetime('now'),
        datetime('now')
    );
