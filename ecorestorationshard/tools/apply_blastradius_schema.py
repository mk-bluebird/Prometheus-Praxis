#!/usr/bin/env python3
"""
Apply blast-radius schema to constellation DB using Python sqlite3 module.

This script applies the canal_blastradius_schema.sql to the canonical
EcoNet constellation database, since the sqlite3 CLI is not available.

Usage:
    python tools/apply_blastradius_schema.py
"""

import sqlite3
from pathlib import Path


def apply_schema():
    # Paths - use /workspace/db/cyboquatic_core.db as the canonical cyboquatic DB
    # (econet_constellation_index.db is a text file, not a real SQLite DB)
    repo_root = Path(__file__).parent.parent
    db_path = repo_root.parent / "db" / "cyboquatic_core.db"
    schema_path = repo_root / "cyboquaticprogress" / "20260724-g-blastradius" / "sql" / "canal_blastradius_schema.sql"
    
    if not db_path.exists():
        print(f"ERROR: Database not found at {db_path}")
        return False
    
    if not schema_path.exists():
        print(f"ERROR: Schema file not found at {schema_path}")
        return False
    
    # Read schema
    with open(schema_path, 'r', encoding='utf-8') as f:
        schema_sql = f.read()
    
    # Connect and apply
    conn = sqlite3.connect(str(db_path))
    cursor = conn.cursor()
    
    try:
        # Enable foreign keys
        cursor.execute("PRAGMA foreign_keys = ON;")
        
        # Execute schema (multi-statement)
        cursor.executescript(schema_sql)
        conn.commit()
        
        # Verify tables were created
        cursor.execute("""
            SELECT name FROM sqlite_master 
            WHERE type='table' AND name IN ('canal_node', 'ker_profile', 'surcharge_event', 'blast_radius_diag')
            ORDER BY name;
        """)
        tables = [row[0] for row in cursor.fetchall()]
        
        print(f"Schema applied successfully to: {db_path}")
        print(f"Tables created/verified: {tables}")
        
        # Show indices
        cursor.execute("""
            SELECT name FROM sqlite_master 
            WHERE type='index' AND tbl_name IN ('canal_node', 'ker_profile', 'surcharge_event', 'blast_radius_diag')
            ORDER BY tbl_name, name;
        """)
        indices = [row[0] for row in cursor.fetchall()]
        print(f"Indices created: {indices}")
        
        return True
        
    except Exception as e:
        print(f"ERROR applying schema: {e}")
        conn.rollback()
        return False
    finally:
        conn.close()


if __name__ == "__main__":
    success = apply_schema()
    exit(0 if success else 1)
