-- Cyboquatic research tasks for always-improve logic.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS researchtask (
    taskid        TEXT PRIMARY KEY,
    code          TEXT NOT NULL,
    title         TEXT NOT NULL,
    description   TEXT NOT NULL,
    repotarget    TEXT NOT NULL,
    filename      TEXT NOT NULL,
    cratename     TEXT NOT NULL,
    priority      INTEGER NOT NULL,
    kerktarget    REAL NOT NULL,
    keretarget    REAL NOT NULL,
    kerrtarget    REAL NOT NULL,
    lanehint      TEXT NOT NULL,
    notes         TEXT
);

INSERT OR REPLACE INTO researchtask (
    taskid,
    code,
    title,
    description,
    repotarget,
    filename,
    cratename,
    priority,
    kerktarget,
    keretarget,
    kerrtarget,
    lanehint,
    notes
) VALUES
    (
        'T05BLASTRADIUSHELPERS',
        'CYBO-BLAST-HELPERS-2026',
        'Cyboquatic blast-radius helpers and views',
        'Implement helper functions and views for hex-encoded blast-radius descriptors and neighbor queries (hops/meters/hours) anchored to vcyboquaticblastradiusrestoration.',
        'eco_restoration_shard',
        'Eco-Fort/dbdbcyboquaticblastrestorationview2026v1.sql',
        'ecorestorationshard',
        8,
        0.94,
        0.91,
        0.12,
        'RESEARCH',
        'Read-only blast-radius adjacency tooling for Cyboquatic nodes; no actuation.'
    ),
    (
        'T06LANETRENDANALYZER',
        'CYBO-LANE-TRENDS-2026',
        'Cyboquatic lane trend analyzer',
        'Compute residual trends per lane and block promotions unless KER bands and Lyapunov conditions are satisfied for Cyboquatic nodes, keeping non-offsettable planes monotone-safe.',
        'eco_restoration_shard',
        'Eco-Fort/crates/governance-guards/src/lib.rs',
        'governance-guards',
        9,
        0.95,
        0.92,
        0.12,
        'RESEARCH',
        'Extend governance-guards to specialize lane admissibility for Cyboquatic planes.'
    ),
    (
        'T02ECOPERJOULEROUTER',
        'CYBO-ECOPERJ-ROUTER-2026',
        'Eco-per-joule aware router (diagnostics)',
        'Add an eco-per-joule aware workload router module that suggests non-actuating node placements minimizing energy per restoration, integrated with CyboquaticEcoPlot and CyboquaticRestorationSurface.',
        'eco_restoration_shard',
        'Eco-Fort/crates/ecorestorationshard/src/lib.rs',
        'ecorestorationshard',
        7,
        0.94,
        0.92,
        0.13,
        'RESEARCH',
        'Router outputs recommendations only (JSON), never actuator commands.'
    );
