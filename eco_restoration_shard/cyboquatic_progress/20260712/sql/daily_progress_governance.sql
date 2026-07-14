-- file: eco_restoration_shard/cyboquatic_progress/20260712/sql/daily_progress_governance.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS daily_progress (
  progressid INTEGER PRIMARY KEY AUTOINCREMENT,
  yyyymmdd TEXT NOT NULL,
  crateid TEXT NOT NULL,
  domain TEXT NOT NULL,
  subtaskid TEXT NOT NULL,
  nodeid TEXT NOT NULL,
  sampleid TEXT NOT NULL,
  timestamputc TEXT NOT NULL,
  evidencehex TEXT NOT NULL,
  kfactor REAL NOT NULL,
  efactor REAL NOT NULL,
  rfactor REAL NOT NULL,
  priorcrateid TEXT NOT NULL,
  didbound TEXT NOT NULL,
  vtafter REAL NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_daily_progress_date ON daily_progress(yyyymmdd);
CREATE INDEX IF NOT EXISTS idx_daily_progress_node_time ON daily_progress(nodeid, timestamputc);

INSERT INTO daily_progress
(yyyymmdd, crateid, domain, subtaskid, nodeid, sampleid, timestamputc, evidencehex, kfactor, efactor, rfactor, priorcrateid, didbound, vtafter)
VALUES
('20260712',
 'cyboquatic_governance_particle_20260712',
 'governance_particle',
 'PHX-GOV-TRIAD-20260712',
 'PHX-GOV-NODE-01',
 'PHX-GOV-SAMPLE-0001',
 '2026-07-12T23:31:00Z',
 '0x20260712PHXGOVTRIAD',
 0.92, 0.88, 0.14,
 'cyboquatic_governance_particle_20260711',
 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
 0.18);
