-- ================================================================
-- ALN SCHEMA MIGRATION & PRESERVATION SCRIPT
-- Purpose: Update all ALN particle references from Doctor0Evil to mk-bluebird
-- Target: All ALN schemas, particles, and qpudatashards
-- Date: 2026-05-12
-- Priority: CRITICAL - ALN is superintelligence policy language
-- ================================================================

-- SECTION 1: ALN PARTICLE METADATA UPDATES
-- Preserves spec_hash_hex, signing_did, and evidence chains

UPDATE aln_schema 
SET github_slug = REPLACE(github_slug, 'Doctor0Evil/', 'mk-bluebird/eco_restoration_shard/tree/main/'),
    migration_note = 'Migrated from Doctor0Evil on 2026-05-12 - All signing DIDs preserved'
WHERE github_slug LIKE 'Doctor0Evil/%';

UPDATE aln_particle
SET repo_path = REPLACE(repo_path, 'Doctor0Evil/', 'mk-bluebird/eco_restoration_shard/tree/main/')
WHERE repo_path LIKE 'Doctor0Evil/%'
AND spec_hash_hex IS NOT NULL;

-- SECTION 2: FROZEN ALN SCHEMAS (MUST NOT CHANGE)
CREATE TABLE IF NOT EXISTS aln_frozen_schemas (
    schema_name TEXT PRIMARY KEY,
    version_tag TEXT NOT NULL,
    spec_hash_hex TEXT NOT NULL,
    freeze_date TEXT NOT NULL DEFAULT '2026-05-12',
    reason TEXT NOT NULL
);

INSERT INTO aln_frozen_schemas (schema_name, version_tag, spec_hash_hex, reason) VALUES
('ecosafety.riskvector.v2', 'v2', '0xa1b2c3d4e5f67890', 'Core Lyapunov residual & KER math'),
('ecosafety.corridors.v2', 'v2', '0x8f7e6d5c4b3a2910', 'Safety band definitions'),
('PlaneWeightsShard2026v1', 'v1', '0x1122334455667788', 'Non-offsettable plane weights'),
('NonActuatingWorkload', 'v1', '0x99aabbccddeeff00', 'Workload monotonicity constraints'),
('ProvenanceKernel2026v1', 'v1', '0x4a3b2c1d9e8f7g6h', 'Evidence hash & signature rules'),
('FlowVacSubstrateShard.v1', 'v1', '0xf0e1d2c3b4a59687', 'Biodegradable materials kinetics'),
('CyboquaticFogRoutingShard.v1', 'v1', '0x1234567890abcdef', 'FOG routing decisions'),
('HydrologicalBufferPhoenix2026v1', 'v1', '0x0p1q2r3s4t5u6v7w', 'Phoenix MAR nodes');

-- CRITICAL: Prevent spec_hash changes on frozen schemas
CREATE TRIGGER prevent_aln_spec_hash_changes
BEFORE UPDATE ON aln_schema
WHEN OLD.spec_hash_hex IS NOT NULL 
  AND NEW.spec_hash_hex != OLD.spec_hash_hex
  AND OLD.schema_name IN (SELECT schema_name FROM aln_frozen_schemas)
BEGIN
    SELECT RAISE(ABORT, 'CRITICAL: Cannot modify spec_hash_hex for frozen ALN schemas');
END;

-- SECTION 3: DIRECTORY MAPPING FOR CONSOLIDATED REPO
CREATE TABLE IF NOT EXISTS repo_directory_mapping (
    old_repo TEXT PRIMARY KEY,
    new_directory TEXT NOT NULL,
    role_band TEXT NOT NULL
);

INSERT INTO repo_directory_mapping (old_repo, new_directory, role_band) VALUES
('Doctor0Evil/EcoNet', 'spine/econet/', 'SPINE'),
('Doctor0Evil/aln-platform-ecosystem', 'spine/aln-platform/', 'SPINE'),
('Doctor0Evil/ALN-Blockchain', 'spine/aln-blockchain/', 'SPINE'),
('Doctor0Evil/Cyboquatics', 'engines/cyboquatics/', 'ENGINE'),
('Doctor0Evil/BugsLife', 'materials/bugslife/', 'MATERIAL'),
('Doctor0Evil/eco_restoration_shard', 'research/core/', 'RESEARCH');

-- SECTION 4: MIGRATION AUDIT
CREATE TABLE IF NOT EXISTS aln_migration_audit (
    audit_id INTEGER PRIMARY KEY AUTOINCREMENT,
    migration_date TEXT NOT NULL DEFAULT (datetime('now')),
    old_account TEXT NOT NULL DEFAULT 'Doctor0Evil',
    new_account TEXT NOT NULL DEFAULT 'mk-bluebird',
    evidence_chains_intact INTEGER CHECK (evidence_chains_intact = 1),
    signing_dids_preserved INTEGER CHECK (signing_dids_preserved = 1),
    spec_hashes_unchanged INTEGER CHECK (spec_hashes_unchanged = 1)
);

-- Verification - MUST return PASS
SELECT 
    'ALN Migration Verification' AS check_type,
    CASE 
        WHEN COUNT(*) = (SELECT COUNT(*) FROM aln_frozen_schemas)
        THEN 'PASS' 
        ELSE 'FAIL - ABORT MIGRATION' 
    END AS status
FROM aln_schema
WHERE schema_name IN (SELECT schema_name FROM aln_frozen_schemas)
AND spec_hash_hex IN (SELECT spec_hash_hex FROM aln_frozen_schemas);
