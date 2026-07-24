#!/usr/bin/env python3
"""
Kernel Policy & Governance Plumbing - SQL Setup Script
Uses Python stdlib sqlite3 to create tables, views, and seed data.
"""

import sqlite3
import os

DB_DIR = "/workspace/Eco-Fort/db"
CORE_DB_PATH = "/workspace/db/cyboquatic_core.db"
PLANEWEIGHTS_DB_PATH = os.path.join(DB_DIR, "planeweights.db")

def setup_planeweights_db():
    """Task 1: Create planeweightsplane table with initial data."""
    os.makedirs(DB_DIR, exist_ok=True)
    
    conn = sqlite3.connect(PLANEWEIGHTS_DB_PATH)
    cursor = conn.cursor()
    
    # Create table
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS planeweightsplane (
            plane_name TEXT PRIMARY KEY,
            weight REAL NOT NULL,
            is_non_compensatable INTEGER NOT NULL CHECK (is_non_compensatable IN (0,1))
        )
    """)
    
    # Insert initial rows
    initial_data = [
        ('water', 1.0, 0),
        ('heat', 1.0, 0),
        ('carbon', 1.0, 1),
        ('topology', 1.0, 1),
    ]
    
    for plane_name, weight, is_non_compensatable in initial_data:
        cursor.execute("""
            INSERT OR IGNORE INTO planeweightsplane (plane_name, weight, is_non_compensatable)
            VALUES (?, ?, ?)
        """, (plane_name, weight, is_non_compensatable))
    
    conn.commit()
    
    # Verify schema and rows
    print("=== Task 1: Plane Weights DB ===")
    print("\nSchema for planeweightsplane:")
    cursor.execute("SELECT sql FROM sqlite_master WHERE type='table' AND name='planeweightsplane';")
    result = cursor.fetchone()
    print(result[0] if result else "Table not found")
    
    print("\nRows in planeweightsplane:")
    cursor.execute("SELECT plane_name, weight, is_non_compensatable FROM planeweightsplane;")
    for row in cursor.fetchall():
        print(f"  {row[0]}: weight={row[1]}, is_non_compensatable={row[2]}")
    
    conn.close()
    print("\nPlaneweights DB created successfully at:", PLANEWEIGHTS_DB_PATH)


def setup_core_db():
    """Tasks 2 & 3: Create views and tables in cyboquatic_core.db."""
    os.makedirs(os.path.dirname(CORE_DB_PATH), exist_ok=True)
    
    conn = sqlite3.connect(CORE_DB_PATH)
    cursor = conn.cursor()
    
    # First, create base tables that the views depend on (shardker, shardresidual)
    # These are minimal stubs so the views can be created
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS shardker (
            frameid TEXT,
            nodeid TEXT,
            plane TEXT,
            k REAL,
            e REAL,
            r REAL,
            fogregionid TEXT,
            fogchannelid TEXT,
            ts TEXT
        )
    """)
    
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS shardresidual (
            frameid TEXT,
            nodeid TEXT,
            plane TEXT,
            vt_before REAL,
            vt_after REAL,
            r_topology REAL,
            ts TEXT
        )
    """)
    
    # Task 2: Create vshardker view
    cursor.execute("""
        CREATE VIEW IF NOT EXISTS vshardker AS
        SELECT frameid, nodeid, plane, k, e, r, (k * e - r) AS ker_score, fogregionid, fogchannelid, ts
        FROM shardker
    """)
    
    # Task 2: Create vshardresidual view
    cursor.execute("""
        CREATE VIEW IF NOT EXISTS vshardresidual AS
        SELECT frameid, nodeid, plane, vt_before, vt_after, (vt_after - vt_before) AS delta_vt, r_topology, ts
        FROM shardresidual
    """)
    
    # Task 3: Create lanestatusshard table
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS lanestatusshard (
            entity_id TEXT,
            lane_name TEXT,
            ker_band TEXT,
            vt_window REAL,
            ts TEXT,
            reason TEXT
        )
    """)
    
    # Task 3: Create virtalaneverdict table
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS virtalaneverdict (
            entity_id TEXT,
            lane_name TEXT,
            verdict_code TEXT,
            verdict_reason TEXT,
            issued_ts TEXT
        )
    """)
    
    # Task 3: Create vlatestlanestatus view
    cursor.execute("""
        CREATE VIEW IF NOT EXISTS vlatestlanestatus AS
        SELECT ls.entity_id, ls.lane_name, ls.ker_band, ls.vt_window, ls.ts, 
               lv.verdict_code, lv.verdict_reason
        FROM lanestatusshard ls
        LEFT JOIN virtalaneverdict lv ON ls.entity_id = lv.entity_id AND ls.lane_name = lv.lane_name
        WHERE ls.ts = (SELECT MAX(ts) FROM lanestatusshard ls2 WHERE ls2.entity_id = ls.entity_id)
    """)
    
    conn.commit()
    
    # Verify schemas
    print("\n=== Task 2: KER and Residual Views ===")
    
    print("\nSchema for vshardker:")
    cursor.execute("SELECT sql FROM sqlite_master WHERE type='view' AND name='vshardker';")
    result = cursor.fetchone()
    print(result[0] if result else "View not found")
    
    print("\nSchema for vshardresidual:")
    cursor.execute("SELECT sql FROM sqlite_master WHERE type='view' AND name='vshardresidual';")
    result = cursor.fetchone()
    print(result[0] if result else "View not found")
    
    print("\n=== Task 3: Lane Status and Verdict Tables ===")
    
    print("\nSchema for lanestatusshard:")
    cursor.execute("SELECT sql FROM sqlite_master WHERE type='table' AND name='lanestatusshard';")
    result = cursor.fetchone()
    print(result[0] if result else "Table not found")
    
    print("\nSchema for virtalaneverdict:")
    cursor.execute("SELECT sql FROM sqlite_master WHERE type='table' AND name='virtalaneverdict';")
    result = cursor.fetchone()
    print(result[0] if result else "Table not found")
    
    print("\nSchema for vlatestlanestatus:")
    cursor.execute("SELECT sql FROM sqlite_master WHERE type='view' AND name='vlatestlanestatus';")
    result = cursor.fetchone()
    print(result[0] if result else "View not found")
    
    conn.close()
    print("\nCore DB created successfully at:", CORE_DB_PATH)


if __name__ == "__main__":
    setup_planeweights_db()
    setup_core_db()
    print("\n=== All SQL tasks completed ===")
