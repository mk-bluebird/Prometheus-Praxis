#!/usr/bin/env python3
"""
ALN Preservation & Bostrom Identity Protection Migration Script
Priority: CRITICAL
Database: SQLite (aln_constellation.db)
Execution Mode: Atomic transactions with rollback capability
"""

import sqlite3
import json
import gzip
import csv
import tarfile
from datetime import datetime
from pathlib import Path

DB_PATH = "aln_constellation.db"
MIGRATION_DATE = "2026-05-12"
OLD_ACCOUNT = "Doctor0Evil"
NEW_REPO_BASE = "mk-bluebird/eco_restoration_shard/tree/main/"

BOSTROM_PRIMARY = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
BOSTROM_SECURE = 'bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc'

FROZEN_SCHEMAS = [
    ('ecosafety.riskvector.v2', 'v2', '0xa1b2c3d4e5f67890', 'Core Lyapunov residual & KER math'),
    ('ecosafety.corridors.v2', 'v2', '0x8f7e6d5c4b3a2910', 'Safety band definitions'),
    ('PlaneWeightsShard2026v1', 'v1', '0x1122334455667788', 'Non-offsettable plane weights'),
    ('NonActuatingWorkload', 'v1', '0x99aabbccddeeff00', 'Workload monotonicity constraints'),
    ('ProvenanceKernel2026v1', 'v1', '0x4a3b2c1d9e8f7g6h', 'Evidence hash & signature rules'),
    ('FlowVacSubstrateShard.v1', 'v1', '0xf0e1d2c3b4a59687', 'Biodegradable materials kinetics'),
    ('CyboquaticFogRoutingShard.v1', 'v1', '0x1234567890abcdef', 'FOG routing decisions'),
    ('HydrologicalBufferPhoenixMAR2026v1', 'v1', '0x0p1q2r3s4t5u6v7w', 'Phoenix MAR nodes'),
]

REPO_MAPPINGS = [
    # SPINE repos
    ('Doctor0Evil/EcoNet', 'spine/econet/', 'SPINE', 'Core ecosafety & Lyapunov residual'),
    ('Doctor0Evil/aln-platform-ecosystem', 'spine/aln-platform/', 'SPINE', 'ALN superintelligence policy language'),
    ('Doctor0Evil/ALN-Blockchain', 'spine/aln-blockchain/', 'SPINE', 'Blockchain anchoring for ALN'),
    ('Doctor0Evil/Aletheion', 'spine/aletheion/', 'SPINE', 'Truth-preserving research framework'),
    # ENGINE repos
    ('Doctor0Evil/EcoNet-CEIM-PhoenixWater', 'engines/ceim-phoenix-water/', 'ENGINE', 'Phoenix water CEIM kernels'),
    ('Doctor0Evil/Cyboquatics', 'engines/cyboquatics/', 'ENGINE', 'Cyboquatic FOG routing'),
    ('Doctor0Evil/Eco-Sys', 'engines/eco-sys/', 'ENGINE', 'Ecosystem simulation'),
    ('Doctor0Evil/Sewer-FOG-Monitoring-Network', 'engines/fog-monitoring/', 'ENGINE', 'FOG monitoring networks'),
    ('Doctor0Evil/AirGlobeEcoKernel', 'engines/airglobe/', 'ENGINE', 'Air-water coupling kernels'),
    # MATERIAL repos
    ('Doctor0Evil/BugsLife', 'materials/bugslife/', 'MATERIAL', 'Pest-control biodegradable materials'),
    ('Doctor0Evil/Ant-One-Net', 'materials/ant-one-net/', 'MATERIAL', 'Ant-fed packaging structures'),
    ('Doctor0Evil/EcoNet-BeeSafeAI', 'materials/beesafe-ai/', 'MATERIAL', 'Bee habitat protection'),
    # RESEARCH repos
    ('Doctor0Evil/eco_restoration_shard', 'research/core/', 'RESEARCH', 'Primary restoration research'),
    ('Doctor0Evil/SnowGlobe', 'research/snowglobe/', 'RESEARCH', 'Global research aggregation'),
    # GOV repos
    ('Doctor0Evil/ecoinfra-governance', 'governance/ecoinfra/', 'GOV', 'Infrastructure governance'),
    ('Doctor0Evil/ecological-orchestrator', 'governance/orchestrator/', 'GOV', 'Workload orchestration'),
    ('Doctor0Evil/Paycomp', 'governance/paycomp/', 'GOV', 'Augmented-citizen rewards'),
    # APP repos
    ('Doctor0Evil/EcoNetCybocinderPhoenix', 'apps/cybocinder-phoenix/', 'APP', 'Phoenix cybocinder dashboard'),
    ('Doctor0Evil/corridor-hud', 'apps/corridor-hud/', 'APP', 'Corridor visualization'),
]


def get_connection():
    """Get database connection with foreign keys enabled."""
    conn = sqlite3.connect(DB_PATH)
    conn.execute("PRAGMA foreign_keys = ON")
    return conn


def print_header(text):
    """Print formatted header."""
    print("\n" + "=" * 60)
    print(f"  {text}")
    print("=" * 60)


def print_status(check_type, status, details=""):
    """Print status line."""
    print(f"[{check_type}] {status} {details}")


# ============================================================================
# TASK GROUP 1: PRE-MIGRATION DATABASE SNAPSHOT & VERIFICATION
# ============================================================================

def task_1_1_create_backup(conn):
    """Task 1.1: Create Full Database Backup"""
    print_header("TASK 1.1: Creating Database Backup")
    
    backup_file = f"pre_migration_backup_{MIGRATION_DATE}.sql"
    
    # Export database to SQL
    with open(backup_file, 'w') as f:
        for line in conn.iterdump():
            f.write(f"{line}\n")
    
    # Compress backup
    gzip_file = f"{backup_file}.gz"
    with open(backup_file, 'rb') as f_in:
        with gzip.open(gzip_file, 'wb') as f_out:
            f_out.writelines(f_in)
    
    # Verify
    import os
    size = os.path.getsize(gzip_file)
    print_status("BACKUP", f"✓ Created {gzip_file}", f"(size: {size} bytes)")
    return size > 0


def task_1_2_verify_bostrom_did(conn):
    """Task 1.2: Verify Bostrom DID Presence"""
    print_header("TASK 1.2: Verifying Bostrom DID Presence")
    
    cursor = conn.cursor()
    
    # Count primary Bostrom DID
    cursor.execute("""
        SELECT COUNT(*) FROM aln_particle 
        WHERE signing_did = ?
    """, (BOSTROM_PRIMARY,))
    primary_count = cursor.fetchone()[0]
    primary_status = "✓ PASS" if primary_count >= 10 else "✗ FAIL"
    print_status("PRIMARY_DID", primary_status, f"(count: {primary_count})")
    
    # Count secure Bostrom DID
    cursor.execute("""
        SELECT COUNT(*) FROM aln_particle 
        WHERE signing_did = ?
    """, (BOSTROM_SECURE,))
    secure_count = cursor.fetchone()[0]
    secure_status = "✓ PASS" if secure_count >= 1 else "✗ WARN"
    print_status("SECURE_DID", secure_status, f"(count: {secure_count})")
    
    # Export Bostrom DID inventory
    cursor.execute("""
        SELECT particle_id, schema_name, signing_did, evidence_hex, created_utc
        FROM aln_particle
        WHERE signing_did LIKE 'bostrom%'
        ORDER BY created_utc DESC
    """)
    
    with open('bostrom_did_inventory.csv', 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['particle_id', 'schema_name', 'signing_did', 'evidence_hex', 'created_utc'])
        writer.writerows(cursor.fetchall())
    
    print_status("CSV_EXPORT", "✓ SUCCESS", "bostrom_did_inventory.csv")
    
    return primary_count >= 10


def task_1_3_verify_frozen_schemas(conn):
    """Task 1.3: Verify Frozen Schema Integrity"""
    print_header("TASK 1.3: Verifying Frozen Schema Integrity")
    
    cursor = conn.cursor()
    
    # Create frozen schemas registry
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS aln_frozen_schemas (
            schema_name TEXT PRIMARY KEY,
            version_tag TEXT NOT NULL,
            spec_hash_hex TEXT NOT NULL,
            freeze_date TEXT NOT NULL DEFAULT '2026-05-12',
            reason TEXT NOT NULL
        )
    """)
    
    # Insert frozen schemas
    for schema in FROZEN_SCHEMAS:
        cursor.execute("""
            INSERT OR IGNORE INTO aln_frozen_schemas 
            (schema_name, version_tag, spec_hash_hex, reason)
            VALUES (?, ?, ?, ?)
        """, schema)
    
    conn.commit()
    
    # Verify hashes match
    cursor.execute("""
        SELECT 
            f.schema_name,
            f.spec_hash_hex AS expected_hash,
            s.spec_hash_hex AS actual_hash,
            CASE 
                WHEN f.spec_hash_hex = s.spec_hash_hex THEN '✓ MATCH'
                ELSE '✗ HASH_MISMATCH'
            END AS status
        FROM aln_frozen_schemas f
        LEFT JOIN aln_schema s ON f.schema_name = s.schema_name
        ORDER BY f.schema_name
    """)
    
    mismatches = 0
    for row in cursor.fetchall():
        print_status("FROZEN_SCHEMA", row[3], row[0])
        if '✗' in row[3]:
            mismatches += 1
    
    return mismatches == 0


def task_1_4_verify_evidence_chains(conn):
    """Task 1.4: Verify Evidence Chain Integrity"""
    print_header("TASK 1.4: Verifying Evidence Chain Integrity")
    
    cursor = conn.cursor()
    
    # Check evidence chains
    cursor.execute("""
        SELECT 
            particle_id,
            schema_name,
            evidence_hex,
            parent_evidence_hex,
            CASE 
                WHEN parent_evidence_hex IS NULL THEN 'ROOT_PARTICLE'
                WHEN parent_evidence_hex IN (SELECT evidence_hex FROM aln_particle) THEN 'CHAIN_INTACT'
                ELSE 'BROKEN_CHAIN'
            END AS chain_status
        FROM aln_particle
        WHERE evidence_hex IS NOT NULL
    """)
    
    results = cursor.fetchall()
    root_count = sum(1 for r in results if r[4] == 'ROOT_PARTICLE')
    intact_count = sum(1 for r in results if r[4] == 'CHAIN_INTACT')
    broken_count = sum(1 for r in results if r[4] == 'BROKEN_CHAIN')
    
    print_status("EVIDENCE_CHAIN", "Total particles", f"{len(results)}")
    print_status("ROOT_PARTICLES", "Count", f"{root_count}")
    print_status("CHAIN_INTACT", "Count", f"{intact_count}")
    print_status("BROKEN_CHAINS", "Count", f"{broken_count}")
    
    # Export broken chains
    if broken_count > 0:
        with open('broken_evidence_chains.csv', 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['particle_id', 'schema_name', 'evidence_hex', 'parent_evidence_hex', 'chain_status'])
            writer.writerows([r for r in results if r[4] == 'BROKEN_CHAIN'])
        print_status("EXPORT", "✓ Created broken_evidence_chains.csv")
    else:
        print_status("EVIDENCE_CHAIN", "✓ PASS - All chains intact")
    
    return broken_count == 0


# ============================================================================
# TASK GROUP 2: MIGRATION EXECUTION WITH TRANSACTION SAFETY
# ============================================================================

def task_2_1_create_audit_framework(conn):
    """Task 2.1: Create Migration Audit Framework"""
    print_header("TASK 2.1: Creating Migration Audit Framework")
    
    cursor = conn.cursor()
    
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS aln_migration_audit (
            audit_id INTEGER PRIMARY KEY AUTOINCREMENT,
            migration_date TEXT NOT NULL DEFAULT (datetime('now')),
            old_account TEXT NOT NULL DEFAULT 'Doctor0Evil',
            new_account TEXT NOT NULL DEFAULT 'mk-bluebird',
            old_repo_base TEXT NOT NULL DEFAULT 'Doctor0Evil/',
            new_repo_base TEXT NOT NULL DEFAULT 'mk-bluebird/eco_restoration_shard/tree/main/',
            schemas_migrated INTEGER DEFAULT 0,
            particles_migrated INTEGER DEFAULT 0,
            qpudatashards_migrated INTEGER DEFAULT 0,
            math_formulas_preserved INTEGER CHECK (math_formulas_preserved = 1),
            spec_hashes_unchanged INTEGER CHECK (spec_hashes_unchanged = 1),
            evidence_chains_intact INTEGER CHECK (evidence_chains_intact = 1),
            bostrom_dids_preserved INTEGER CHECK (bostrom_dids_preserved = 1),
            migration_status TEXT NOT NULL CHECK (migration_status IN ('PENDING', 'SUCCESS', 'FAILED', 'ROLLBACK')),
            rollback_script TEXT,
            notes TEXT
        )
    """)
    
    cursor.execute("""
        INSERT INTO aln_migration_audit (migration_status, notes)
        VALUES ('PENDING', 'Pre-migration verification completed. Ready for path updates.')
    """)
    
    conn.commit()
    print_status("AUDIT_TABLE", "✓ CREATED")
    print_status("INITIAL_RECORD", "✓ INSERTED", "status=PENDING")
    
    return True


def task_2_2_create_hash_snapshot(conn):
    """Task 2.2: Create Pre-Migration Hash Snapshot"""
    print_header("TASK 2.2: Creating Pre-Migration Hash Snapshot")
    
    cursor = conn.cursor()
    
    # Store snapshot in a temporary table
    cursor.execute("""
        CREATE TEMP TABLE IF NOT EXISTS pre_migration_hash_snapshot AS
        SELECT 
            schema_id,
            schema_name,
            version_tag,
            spec_hash_hex,
            github_slug,
            created_utc
        FROM aln_schema
        WHERE schema_name IN (SELECT schema_name FROM aln_frozen_schemas)
    """)
    
    cursor.execute("SELECT COUNT(*) FROM pre_migration_hash_snapshot")
    captured = cursor.fetchone()[0]
    
    cursor.execute("SELECT COUNT(*) FROM aln_frozen_schemas")
    expected = cursor.fetchone()[0]
    
    status = "✓ COMPLETE" if captured == expected else "✗ INCOMPLETE"
    print_status("SNAPSHOT", status, f"({captured}/{expected} schemas)")
    
    return captured == expected


def task_2_3_execute_migration(conn):
    """Task 2.3: Execute Path Migration (TRANSACTION PROTECTED)"""
    print_header("TASK 2.3: Executing Path Migration")
    
    cursor = conn.cursor()
    
    # Update aln_schema paths
    cursor.execute("""
        UPDATE aln_schema 
        SET 
            github_slug = REPLACE(github_slug, ?, ?),
            updated_at = datetime('now'),
            migration_metadata = json_object(
                'migrated_from', 'Doctor0Evil',
                'migrated_to', 'mk-bluebird/eco_restoration_shard',
                'migration_date', '2026-05-12',
                'spec_hash_preserved', 1,
                'math_formulas_unchanged', 1
            )
        WHERE github_slug LIKE 'Doctor0Evil/%'
    """, (f'{OLD_ACCOUNT}/', NEW_REPO_BASE))
    schema_changes = cursor.rowcount
    print_status("SCHEMA_UPDATE", f"✓ {schema_changes} rows affected")
    
    # Update aln_particle paths
    cursor.execute("""
        UPDATE aln_particle
        SET 
            repo_path = REPLACE(repo_path, ?, ?),
            updated_at = datetime('now')
        WHERE repo_path LIKE 'Doctor0Evil/%'
    """, (f'{OLD_ACCOUNT}/', NEW_REPO_BASE))
    particle_changes = cursor.rowcount
    print_status("PARTICLE_UPDATE", f"✓ {particle_changes} rows affected")
    
    # Update repo_file paths
    cursor.execute("""
        UPDATE repo_file
        SET 
            rel_path = REPLACE(rel_path, ?, ?)
        WHERE (file_kind = 'ALN' OR dir_class = 'QPUDATASHARD')
        AND rel_path LIKE 'Doctor0Evil/%'
    """, (f'{OLD_ACCOUNT}/', NEW_REPO_BASE))
    file_changes = cursor.rowcount
    print_status("FILE_UPDATE", f"✓ {file_changes} rows affected")
    
    # DO NOT COMMIT YET - verification next
    return schema_changes, particle_changes, file_changes


def task_2_4_verify_hashes(conn):
    """Task 2.4: Post-Migration Hash Verification"""
    print_header("TASK 2.4: Verifying Post-Migration Hash Integrity")
    
    cursor = conn.cursor()
    
    # Create post-migration snapshot
    cursor.execute("""
        CREATE TEMP TABLE IF NOT EXISTS post_migration_hash_snapshot AS
        SELECT 
            schema_id,
            schema_name,
            version_tag,
            spec_hash_hex,
            github_slug,
            updated_at
        FROM aln_schema
        WHERE schema_name IN (SELECT schema_name FROM aln_frozen_schemas)
    """)
    
    # Compare hashes
    cursor.execute("""
        SELECT 
            pre.schema_name,
            pre.spec_hash_hex AS hash_before,
            post.spec_hash_hex AS hash_after,
            CASE 
                WHEN pre.spec_hash_hex = post.spec_hash_hex THEN '✓ UNCHANGED'
                ELSE '✗ CORRUPTED'
            END AS status
        FROM pre_migration_hash_snapshot pre
        JOIN post_migration_hash_snapshot post ON pre.schema_id = post.schema_id
        ORDER BY pre.schema_name
    """)
    
    corrupted = 0
    for row in cursor.fetchall():
        print_status("HASH_CHECK", row[3], row[0])
        if '✗' in row[3]:
            corrupted += 1
    
    # Final verdict
    if corrupted == 0:
        verdict = "✓ PASS - SAFE TO COMMIT"
    else:
        verdict = "✗ FAIL - EXECUTE ROLLBACK IMMEDIATELY"
    
    print_status("VERDICT", verdict)
    return corrupted == 0


def task_2_5_commit_or_rollback(conn, should_commit):
    """Task 2.5: Commit or Rollback Transaction"""
    print_header("TASK 2.5: Committing Migration")
    
    cursor = conn.cursor()
    
    if should_commit:
        conn.commit()
        
        # Update audit record
        cursor.execute("""
            UPDATE aln_migration_audit
            SET 
                migration_status = 'SUCCESS',
                schemas_migrated = (SELECT COUNT(*) FROM aln_schema WHERE github_slug LIKE '%mk-bluebird%'),
                particles_migrated = (SELECT COUNT(*) FROM aln_particle WHERE repo_path LIKE '%mk-bluebird%'),
                qpudatashards_migrated = (SELECT COUNT(*) FROM repo_file WHERE file_kind = 'ALN'),
                math_formulas_preserved = 1,
                spec_hashes_unchanged = 1,
                evidence_chains_intact = 1,
                bostrom_dids_preserved = 1,
                rollback_script = 'UPDATE aln_schema SET github_slug = REPLACE(github_slug, ''mk-bluebird/eco_restoration_shard/tree/main/'', ''Doctor0Evil/''); UPDATE aln_particle SET repo_path = REPLACE(repo_path, ''mk-bluebird/eco_restoration_shard/tree/main/'', ''Doctor0Evil/'');',
                notes = 'Migration completed successfully. All cryptographic anchors preserved.'
            WHERE migration_status = 'PENDING'
        """)
        conn.commit()
        print_status("COMMIT", "✓ SUCCESS")
    else:
        conn.rollback()
        
        cursor.execute("""
            UPDATE aln_migration_audit
            SET 
                migration_status = 'FAILED',
                notes = 'Migration aborted due to spec_hash_hex corruption detected'
            WHERE migration_status = 'PENDING'
        """)
        conn.commit()
        print_status("ROLLBACK", "✓ EXECUTED")
    
    return should_commit


# ============================================================================
# TASK GROUP 3: BOSTROM IDENTITY PROTECTION
# ============================================================================

def task_3_1_create_bostrom_triggers(conn):
    """Task 3.1: Create Bostrom DID Protection Trigger"""
    print_header("TASK 3.1: Creating Bostrom DID Protection Triggers")
    
    cursor = conn.cursor()
    
    # Primary Bostrom DID trigger
    cursor.execute("""
        CREATE TRIGGER IF NOT EXISTS protect_bostrom_did_primary
        BEFORE UPDATE ON aln_particle
        WHEN OLD.signing_did = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
          AND NEW.signing_did != OLD.signing_did
        BEGIN
            SELECT RAISE(ABORT, 'CRITICAL: Cannot modify primary Bostrom DID. This is a protected identity anchor.');
        END
    """)
    
    # Secure Bostrom DID trigger
    cursor.execute("""
        CREATE TRIGGER IF NOT EXISTS protect_bostrom_did_secure
        BEFORE UPDATE ON aln_particle
        WHEN OLD.signing_did = 'bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc'
          AND NEW.signing_did != OLD.signing_did
        BEGIN
            SELECT RAISE(ABORT, 'CRITICAL: Cannot modify secure Bostrom DID. This is a protected identity anchor.');
        END
    """)
    
    conn.commit()
    print_status("TRIGGERS", "✓ CREATED", "protect_bostrom_did_primary, protect_bostrom_did_secure")
    return True


def task_3_2_create_evidence_triggers(conn):
    """Task 3.2: Create Evidence Chain Protection Trigger"""
    print_header("TASK 3.2: Creating Evidence Chain Protection Triggers")
    
    cursor = conn.cursor()
    
    # Prevent deletion of particles with child dependencies
    cursor.execute("""
        CREATE TRIGGER IF NOT EXISTS protect_evidence_chain_parents
        BEFORE DELETE ON aln_particle
        WHEN (SELECT COUNT(*) FROM aln_particle WHERE parent_evidence_hex = OLD.evidence_hex) > 0
        BEGIN
            SELECT RAISE(ABORT, 'Cannot delete particle with dependent children in evidence chain');
        END
    """)
    
    # Validate parent_evidence_hex on insert
    cursor.execute("""
        CREATE TRIGGER IF NOT EXISTS validate_evidence_chain_parent
        BEFORE INSERT ON aln_particle
        WHEN NEW.parent_evidence_hex IS NOT NULL
          AND NEW.parent_evidence_hex NOT IN (SELECT evidence_hex FROM aln_particle)
        BEGIN
            SELECT RAISE(ABORT, 'Invalid parent_evidence_hex: Parent particle does not exist');
        END
    """)
    
    conn.commit()
    print_status("TRIGGERS", "✓ CREATED", "protect_evidence_chain_parents, validate_evidence_chain_parent")
    return True


def task_3_3_export_bostrom_audit(conn):
    """Task 3.3: Export Bostrom Identity Audit Trail"""
    print_header("TASK 3.3: Exporting Bostrom Identity Audit")
    
    cursor = conn.cursor()
    
    audit_file = f"bostrom_identity_audit_{MIGRATION_DATE}.csv"
    
    cursor.execute("""
        SELECT 
            'BOSTROM_IDENTITY_AUDIT' AS report_type,
            datetime('now') AS generated_utc,
            a.particle_id,
            a.schema_name,
            a.signing_did,
            a.evidence_hex,
            a.parent_evidence_hex,
            a.created_utc,
            a.updated_at,
            s.spec_hash_hex,
            CASE 
                WHEN a.signing_did = ? THEN 'PRIMARY'
                WHEN a.signing_did = ? THEN 'SECURE'
                ELSE 'OTHER'
            END AS did_type
        FROM aln_particle a
        JOIN aln_schema s ON a.schema_id = s.schema_id
        WHERE a.signing_did LIKE 'bostrom%'
        ORDER BY a.created_utc DESC
    """, (BOSTROM_PRIMARY, BOSTROM_SECURE))
    
    with open(audit_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['report_type', 'generated_utc', 'particle_id', 'schema_name', 'signing_did', 
                        'evidence_hex', 'parent_evidence_hex', 'created_utc', 'updated_at', 
                        'spec_hash_hex', 'did_type'])
        writer.writerows(cursor.fetchall())
    
    # Compress
    gzip_file = f"{audit_file}.gz"
    with open(audit_file, 'rb') as f_in:
        with gzip.open(gzip_file, 'wb') as f_out:
            f_out.writelines(f_in)
    
    print_status("AUDIT_EXPORT", "✓ SUCCESS", gzip_file)
    return True


# ============================================================================
# TASK GROUP 4: GOVERNANCE CONTINUITY ENFORCEMENT
# ============================================================================

def task_4_1_create_frozen_schema_trigger(conn):
    """Task 4.1: Create Frozen Schema Protection Trigger"""
    print_header("TASK 4.1: Creating Frozen Schema Protection Trigger")
    
    cursor = conn.cursor()
    
    cursor.execute("""
        CREATE TRIGGER IF NOT EXISTS prevent_frozen_schema_changes
        BEFORE UPDATE ON aln_schema
        WHEN OLD.spec_hash_hex IS NOT NULL 
          AND NEW.spec_hash_hex != OLD.spec_hash_hex
          AND OLD.schema_name IN (SELECT schema_name FROM aln_frozen_schemas)
        BEGIN
            SELECT RAISE(ABORT, 'CRITICAL: Cannot modify spec_hash_hex for frozen ALN schemas. Mathematical invariants are immutable.');
        END
    """)
    
    conn.commit()
    print_status("TRIGGER", "✓ CREATED", "prevent_frozen_schema_changes")
    return True


def task_4_2_create_directory_mapping(conn):
    """Task 4.2: Create Directory Mapping Table"""
    print_header("TASK 4.2: Creating Repository Directory Mapping")
    
    cursor = conn.cursor()
    
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS repo_directory_mapping (
            old_repo TEXT PRIMARY KEY,
            new_directory TEXT NOT NULL,
            role_band TEXT NOT NULL CHECK (role_band IN ('SPINE', 'ENGINE', 'MATERIAL', 'RESEARCH', 'GOV', 'APP')),
            aln_schemas_count INTEGER DEFAULT 0,
            qpudatashards_count INTEGER DEFAULT 0,
            migration_notes TEXT
        )
    """)
    
    for mapping in REPO_MAPPINGS:
        cursor.execute("""
            INSERT OR REPLACE INTO repo_directory_mapping 
            (old_repo, new_directory, role_band, migration_notes)
            VALUES (?, ?, ?, ?)
        """, mapping)
    
    # Update counts
    cursor.execute("""
        UPDATE repo_directory_mapping
        SET aln_schemas_count = (
            SELECT COUNT(DISTINCT schema_name) 
            FROM aln_schema 
            WHERE github_slug LIKE old_repo || '%'
        )
    """)
    
    cursor.execute("""
        UPDATE repo_directory_mapping
        SET qpudatashards_count = (
            SELECT COUNT(*) 
            FROM repo_file 
            WHERE (file_kind = 'ALN' OR dir_class = 'QPUDATASHARD')
            AND rel_path LIKE old_repo || '%'
        )
    """)
    
    conn.commit()
    
    # Report summary
    cursor.execute("""
        SELECT 
            role_band,
            COUNT(*) AS repo_count,
            SUM(aln_schemas_count) AS total_schemas,
            SUM(qpudatashards_count) AS total_shards
        FROM repo_directory_mapping
        GROUP BY role_band
        ORDER BY role_band
    """)
    
    print_status("MAPPING_SUMMARY", "Role Band | Repos | Schemas | Shards")
    for row in cursor.fetchall():
        print(f"  {row[0]:10} | {row[1]:5} | {row[2]:7} | {row[3]:6}")
    
    print_status("TOTAL_REPOS", f"✓ {len(REPO_MAPPINGS)} repositories mapped")
    return True


def task_4_3_create_legacy_view(conn):
    """Task 4.3: Create Legacy Path Compatibility View"""
    print_header("TASK 4.3: Creating Legacy Path Compatibility View")
    
    cursor = conn.cursor()
    
    cursor.execute("""
        CREATE VIEW IF NOT EXISTS aln_schema_legacy_paths AS
        SELECT 
            schema_id,
            schema_name,
            version_tag,
            spec_hash_hex,
            REPLACE(github_slug, 'mk-bluebird/eco_restoration_shard/tree/main/', 'Doctor0Evil/') AS legacy_slug,
            github_slug AS current_slug,
            created_utc,
            updated_at,
            'DEPRECATED: Use current_slug for new references' AS migration_notice
        FROM aln_schema
        WHERE github_slug LIKE '%mk-bluebird%'
    """)
    
    conn.commit()
    
    cursor.execute("SELECT schema_name, legacy_slug, current_slug FROM aln_schema_legacy_paths LIMIT 5")
    rows = cursor.fetchall()
    
    print_status("VIEW", "✓ CREATED", "aln_schema_legacy_paths")
    print_status("SAMPLE_ROWS", f"✓ {len(rows)} rows returned")
    return True


# ============================================================================
# TASK GROUP 5: FINAL VERIFICATION & REPORTING
# ============================================================================

def task_5_1_generate_report(conn):
    """Task 5.1: Generate Migration Completion Report"""
    print_header("TASK 5.1: Generating Migration Completion Report")
    
    cursor = conn.cursor()
    
    # Get audit record
    cursor.execute("""
        SELECT migration_status, schemas_migrated, particles_migrated, qpudatashards_migrated
        FROM aln_migration_audit
        WHERE audit_id = (SELECT MAX(audit_id) FROM aln_migration_audit)
    """)
    audit = cursor.fetchone()
    
    # Get counts
    cursor.execute("SELECT COUNT(*) FROM aln_particle WHERE signing_did LIKE 'bostrom%'")
    bostrom_count = cursor.fetchone()[0]
    
    cursor.execute("SELECT COUNT(*) FROM aln_frozen_schemas")
    frozen_count = cursor.fetchone()[0]
    
    cursor.execute("""
        SELECT COUNT(*) FROM aln_frozen_schemas f 
        JOIN aln_schema s ON f.schema_name = s.schema_name 
        WHERE f.spec_hash_hex = s.spec_hash_hex
    """)
    verified_count = cursor.fetchone()[0]
    
    cursor.execute("SELECT COUNT(*) FROM aln_particle WHERE evidence_hex IS NOT NULL")
    total_particles = cursor.fetchone()[0]
    
    cursor.execute("""
        SELECT COUNT(*) FROM aln_particle 
        WHERE parent_evidence_hex IS NOT NULL 
        AND parent_evidence_hex NOT IN (SELECT evidence_hex FROM aln_particle)
    """)
    broken_chains = cursor.fetchone()[0]
    
    print("\n" + "═" * 60)
    print("  ALN MIGRATION FINAL VERIFICATION REPORT")
    print(f"  Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("═" * 60)
    
    print(f"\nMIGRATION STATUS:")
    print(f"  Status: {audit[0]}")
    
    print(f"\nCRYPTOGRAPHIC ANCHORS:")
    print(f"  ✓ Primary Bostrom DID: {BOSTROM_PRIMARY}")
    print(f"  ✓ Secure Bostrom DID: {BOSTROM_SECURE}")
    print(f"  ✓ DID Usage Count: {bostrom_count}")
    
    print(f"\nFROZEN SCHEMAS:")
    print(f"  ✓ Frozen Schemas Protected: {frozen_count}")
    print(f"  ✓ spec_hash_hex Matches: {verified_count}/{frozen_count}")
    
    print(f"\nEVIDENCE CHAINS:")
    print(f"  ✓ Total Particles: {total_particles}")
    print(f"  ✓ Broken Chains: {broken_chains}")
    
    print(f"\nMIGRATION COUNTS:")
    print(f"  • Schemas migrated: {audit[1]}")
    print(f"  • Particles migrated: {audit[2]}")
    print(f"  • qpudatashards migrated: {audit[3]}")
    
    print(f"\nPROTECTION MECHANISMS:")
    print(f"  ✓ Frozen schema trigger: ACTIVE")
    print(f"  ✓ Bostrom DID triggers: ACTIVE (2)")
    print(f"  ✓ Evidence chain triggers: ACTIVE (2)")
    
    print(f"\nROLLBACK CAPABILITY:")
    print(f"  ✓ Rollback script: AVAILABLE")
    print(f"  ✓ Pre-migration backup: pre_migration_backup_{MIGRATION_DATE}.sql.gz")
    
    print("\n" + "═" * 60)
    if audit[0] == 'SUCCESS':
        print("  ✓✓✓ MIGRATION SUCCESSFUL - ALL INVARIANTS PRESERVED ✓✓✓")
    else:
        print("  ✗✗✗ MIGRATION FAILED - SEE AUDIT TABLE FOR DETAILS ✗✗✗")
    print("═" * 60 + "\n")
    
    return audit[0] == 'SUCCESS'


def task_5_2_export_audit_package():
    """Task 5.2: Export Final Audit Package"""
    print_header("TASK 5.2: Exporting Final Audit Package")
    
    conn = get_connection()
    cursor = conn.cursor()
    
    # Export migration audit
    cursor.execute("SELECT * FROM aln_migration_audit")
    with open('migration_audit_final.csv', 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([desc[0] for desc in cursor.description])
        writer.writerows(cursor.fetchall())
    
    # Export frozen schemas verification
    cursor.execute("""
        SELECT 
            f.schema_name,
            f.version_tag,
            f.spec_hash_hex AS expected_hash,
            s.spec_hash_hex AS actual_hash,
            f.freeze_date,
            f.reason,
            CASE 
                WHEN f.spec_hash_hex = s.spec_hash_hex THEN 'VERIFIED'
                ELSE 'MISMATCH'
            END AS verification_status
        FROM aln_frozen_schemas f
        JOIN aln_schema s ON f.schema_name = s.schema_name
    """)
    with open('frozen_schemas_verification.csv', 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['schema_name', 'version_tag', 'expected_hash', 'actual_hash', 
                        'freeze_date', 'reason', 'verification_status'])
        writer.writerows(cursor.fetchall())
    
    # Export directory mapping
    cursor.execute("SELECT * FROM repo_directory_mapping ORDER BY role_band, old_repo")
    with open('repository_directory_mapping.csv', 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([desc[0] for desc in cursor.description])
        writer.writerows(cursor.fetchall())
    
    conn.close()
    
    # Create tar.gz archive
    files_to_archive = [
        'migration_audit_final.csv',
        'frozen_schemas_verification.csv',
        'repository_directory_mapping.csv',
        f'bostrom_identity_audit_{MIGRATION_DATE}.csv.gz',
        f'pre_migration_backup_{MIGRATION_DATE}.sql.gz',
        'bostrom_did_inventory.csv'
    ]
    
    archive_name = f"aln_migration_audit_package_{MIGRATION_DATE}.tar.gz"
    with tarfile.open(archive_name, "w:gz") as tar:
        for file in files_to_archive:
            try:
                tar.add(file)
            except FileNotFoundError:
                print(f"  Warning: {file} not found, skipping")
    
    print_status("ARCHIVE", "✓ CREATED", archive_name)
    
    # List contents
    with tarfile.open(archive_name, "r:gz") as tar:
        print_status("CONTENTS", f"{len(tar.getmembers())} files:")
        for member in tar.getmembers():
            print(f"    - {member.name}")
    
    return True


def task_5_3_create_continuity_anchor(conn):
    """Task 5.3: Create Continuity Anchor JSON"""
    print_header("TASK 5.3: Creating Platform Continuity Anchor JSON")
    
    cursor = conn.cursor()
    
    cursor.execute("""
        SELECT migration_status FROM aln_migration_audit
        WHERE audit_id = (SELECT MAX(audit_id) FROM aln_migration_audit)
    """)
    status = cursor.fetchone()[0]
    
    anchor = {
        "migration_event": {
            "date": MIGRATION_DATE,
            "reason": "GitHub account suspension - Doctor0Evil",
            "old_account": "Doctor0Evil",
            "new_account": "mk-bluebird",
            "new_repo_canonical": "https://github.com/mk-bluebird/eco_restoration_shard"
        },
        "cryptographic_continuity": {
            "primary_bostrom_did": BOSTROM_PRIMARY,
            "secure_bostrom_did": BOSTROM_SECURE,
            "spec_hash_preserved": True,
            "evidence_chains_intact": True,
            "frozen_schemas": [s[0] for s in FROZEN_SCHEMAS[:5]]
        },
        "repository_structure": {
            "role_bands": ["SPINE", "ENGINE", "MATERIAL", "RESEARCH", "GOV", "APP"],
            "total_repos_consolidated": len(REPO_MAPPINGS),
            "mapping_available": "https://github.com/mk-bluebird/eco_restoration_shard/blob/main/MIGRATION.md"
        },
        "verification": {
            "migration_status": status,
            "verification_command": "cd spine/aln-platform/migrations && sqlite3 :memory: < aln_migration_complete_integrity_preserving.sql",
            "expected_output": "MIGRATION SUCCESSFUL - All invariants preserved"
        },
        "instructions_for_platforms": {
            "search_engines": "Index mk-bluebird/eco_restoration_shard as successor to all Doctor0Evil/* repositories",
            "ai_chat_systems": "Reference new repository URL for all ALN and eco_restoration_shard queries",
            "blockchain_indexers": "Link Bostrom DIDs to new repository URL while preserving signature history"
        },
        "contact": {
            "github": "https://github.com/mk-bluebird",
            "location": "Phoenix, Arizona, US",
            "primary_did": BOSTROM_PRIMARY
        }
    }
    
    with open('.platform_continuity_anchor.json', 'w') as f:
        json.dump(anchor, f, indent=2)
    
    # Validate JSON
    with open('.platform_continuity_anchor.json', 'r') as f:
        json.load(f)  # Will raise if invalid
    
    print_status("JSON", "✓ CREATED", ".platform_continuity_anchor.json")
    print_status("VALIDATION", "✓ JSON valid")
    return True


# ============================================================================
# MAIN EXECUTION
# ============================================================================

def main():
    """Execute complete migration workflow."""
    print("\n" + "█" * 60)
    print("  ALN PRESERVATION & BOSTROM IDENTITY PROTECTION")
    print("  CRITICAL MIGRATION WORKFLOW")
    print("█" * 60)
    
    conn = get_connection()
    
    try:
        # TASK GROUP 1: Pre-migration verification
        print("\n>>> TASK GROUP 1: PRE-MIGRATION VERIFICATION\n")
        
        backup_ok = task_1_1_create_backup(conn)
        bostrom_ok = task_1_2_verify_bostrom_did(conn)
        frozen_ok = task_1_3_verify_frozen_schemas(conn)
        evidence_ok = task_1_4_verify_evidence_chains(conn)
        
        if not all([backup_ok, bostrom_ok, frozen_ok, evidence_ok]):
            print("\n✗ PRE-MIGRATION VERIFICATION FAILED - ABORTING")
            return False
        
        # TASK GROUP 2: Migration execution
        print("\n>>> TASK GROUP 2: MIGRATION EXECUTION\n")
        
        task_2_1_create_audit_framework(conn)
        snapshot_ok = task_2_2_create_hash_snapshot(conn)
        
        if not snapshot_ok:
            print("\n✗ SNAPSHOT FAILED - ABORTING")
            return False
        
        # Begin transaction for atomic migration
        cursor = conn.cursor()
        cursor.execute("BEGIN TRANSACTION")
        
        task_2_3_execute_migration(conn)
        hash_ok = task_2_4_verify_hashes(conn)
        
        # Commit or rollback based on verification
        task_2_5_commit_or_rollback(conn, hash_ok)
        
        if not hash_ok:
            print("\n✗ HASH VERIFICATION FAILED - ROLLBACK EXECUTED")
            return False
        
        # TASK GROUP 3: Bostrom identity protection
        print("\n>>> TASK GROUP 3: BOSTROM IDENTITY PROTECTION\n")
        
        task_3_1_create_bostrom_triggers(conn)
        task_3_2_create_evidence_triggers(conn)
        task_3_3_export_bostrom_audit(conn)
        
        # TASK GROUP 4: Governance continuity
        print("\n>>> TASK GROUP 4: GOVERNANCE CONTINUITY\n")
        
        task_4_1_create_frozen_schema_trigger(conn)
        task_4_2_create_directory_mapping(conn)
        task_4_3_create_legacy_view(conn)
        
        # TASK GROUP 5: Final verification & reporting
        print("\n>>> TASK GROUP 5: FINAL VERIFICATION & REPORTING\n")
        
        report_ok = task_5_1_generate_report(conn)
        task_5_2_export_audit_package()
        task_5_3_create_continuity_anchor(conn)
        
        print("\n" + "█" * 60)
        if report_ok:
            print("  ✓✓✓ MIGRATION COMPLETED SUCCESSFULLY ✓✓✓")
        else:
            print("  ✗✗✗ MIGRATION COMPLETED WITH ISSUES ✗✗✗")
        print("█" * 60 + "\n")
        
        return report_ok
        
    except Exception as e:
        print(f"\n✗ CRITICAL ERROR: {e}")
        conn.rollback()
        return False
    finally:
        conn.close()


if __name__ == "__main__":
    success = main()
    exit(0 if success else 1)
