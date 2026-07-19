-- filename: ecorestorationshard/cyboquaticprogress/20260718/sql/cyboquatic_daily_progress_20260718.sql
-- destination: ecorestorationshard/cyboquaticprogress/db_cyboquatic_daily_progress.sqlite
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
-- purpose: Schema extension and seed row for domain (d) workload energyreqJ / deltaVt.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. dailyprogress table (if not already created, compatible extension).
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS dailyprogress (
    progressid     INTEGER PRIMARY KEY AUTOINCREMENT,
    yyyymmdd       TEXT    NOT NULL,
    domain         TEXT    NOT NULL,
    subtaskid      TEXT    NOT NULL,
    nodeid         TEXT    NOT NULL,
    energyreqJ     REAL    NOT NULL,
    deltaVt        REAL    NOT NULL,
    kscore         REAL    NOT NULL,
    escore         REAL    NOT NULL,
    rscore         REAL    NOT NULL,
    evidencehex    TEXT    NOT NULL,
    signingdid     TEXT    NOT NULL,
    prioranchorid  TEXT,
    createdutc     TEXT    NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_dailyprogress_date_domain
    ON dailyprogress (yyyymmdd, domain);

CREATE INDEX IF NOT EXISTS idx_dailyprogress_node
    ON dailyprogress (nodeid);

----------------------------------------------------------------------
-- 2. Seed row for 2026-07-18 workload energyreqJ / deltaVt.
----------------------------------------------------------------------

INSERT INTO dailyprogress (
    yyyymmdd,
    domain,
    subtaskid,
    nodeid,
    energyreqJ,
    deltaVt,
    kscore,
    escore,
    rscore,
    evidencehex,
    signingdid,
    prioranchorid,
    createdutc
) VALUES (
    '20260718',
    'WORKLOADENERGYDV',
    'PHX-WORKLOAD-ENERGYREQDV-2026-07-18',
    'AI-RACK-PHX-01',
    12.5 * 3.6e6,
    -0.05,
    0.95,
    0.91,
    0.12,
    '0x20260718PHXWORKLOADENERGYREQDV',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'PHXWORKLOADENERGYDV20260709',
    '2026-07-18T23:26:00Z'
);
