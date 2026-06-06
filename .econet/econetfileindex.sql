-- filename: .econet/econetfileindex.sql
-- destination: eco_restoration_shard/.econet/econetfileindex.sql

INSERT OR IGNORE INTO econetfileindex
  (filename,                     destination,                                       repotarget,              roleband, lanedefault, description,                                                                                  createdutc,     updatedutc)
VALUES
  ('db_ecoconnect_spine.sql',    'eco_restoration_shard/db/db_ecoconnect_spine.sql','eco_restoration_shard','SPINE',  'RESEARCH',
   'Non-actuating eco-connectivity spine for circuit theory, APSP, and edge telemetry.',                                                           datetime('now'), datetime('now')),
  ('db_dkg_spine.sql',           'eco_restoration_shard/db/db_dkg_spine.sql',       'eco_restoration_shard','SPINE',  'RESEARCH',
   'Non-actuating DKG mirror of Bostrom/IPFS particles, cyberlinks, and Cyberrank.',                                                                datetime('now'), datetime('now')),
  ('db_terminal_spine.sql',      'eco_restoration_shard/db/db_terminal_spine.sql',  'eco_restoration_shard','SPINE',  'RESEARCH',
   'Evidence-only spine for DBSC, CEA, and AAL session protections.',                                                                            datetime('now'), datetime('now')),
  ('db_pqc_crypto_spine.sql',    'eco_restoration_shard/db/db_pqc_crypto_spine.sql','eco_restoration_shard','SPINE',  'RESEARCH',
   'PQC capability and CosmWasm invariants registry (ALN-backed, non-actuating).',                                                                datetime('now'), datetime('now'));
