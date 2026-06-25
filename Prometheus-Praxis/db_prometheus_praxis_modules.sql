-- Prometheus-Praxis Module Registry
-- Grounding: EU Nature Restoration Regulation implementation tracking
-- https://environment.ec.europa.eu/topics/nature-and-biodiversity/nature-restoration-regulation_en

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS prometheus_module_registry (
  module_id TEXT PRIMARY KEY,
  canonical_name TEXT NOT NULL,
  pillar TEXT NOT NULL CHECK (pillar IN ('Eco-Work','Psychological Continuity','Smart-City','Cybernetics','Healthcare','Governance','Cosmic')),
  aln_shard_ref TEXT NOT NULL,
  rust_crate_path TEXT NOT NULL,
  roh_ceiling REAL NOT NULL DEFAULT 0.3,
  neurorights_required INTEGER NOT NULL DEFAULT 1,
  source_url TEXT,
  active INTEGER NOT NULL DEFAULT 1
);

INSERT OR IGNORE INTO prometheus_module_registry VALUES
('dyus-archon','Dyēus-Archon','Governance','dyus-archon.v1','src/governance/dyus_archon.rs',0.3,1,'https://www.iso.org/standard/42001',1),
('tellus-synoptikos','Tellus-Synoptikos','Eco-Work','tellus-synoptikos.v1','src/eco/tellus_synoptikos.rs',0.3,1,'https://www.decadeonrestoration.org/',1),
('summus-civitas','Summus-Civitas','Smart-City','summus-civitas.v1','src/city/summus_civitas.rs',0.3,1,'https://www.pwc.com/m1/en/publications/2025/docs/cognitive-city-operating-system-future-cities.pdf',1),
('hestia-continuus','Hestia-Continuus','Psychological Continuity','hestia-continuus.v1','src/health/hestia_continuus.rs',0.3,1,'https://www.fda.gov/medical-devices/how-study-and-market-your-device/breakthrough-devices-program',1),
('perknos-nexus','Perkūnos-Nexus','Cybernetics','perknos-nexus.v1','src/cybernetics/perknos_nexus.rs',0.3,1,'https://www.nist.gov/itl/ai-risk-management-framework',1),
('gaia-sentinel','Gaia Sentinel','Cosmic','gaia-sentinel.v1','src/cosmic/gaia_sentinel.rs',0.3,1,'https://www.decadeonrestoration.org/',1);
