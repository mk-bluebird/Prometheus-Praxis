#!/usr/bin/env python3
"""
Verification script for governance wiring.
Tests:
1. governance_query_audit table and view
2. actuation_request table and trigger
3. kg_node, kg_edge, kg_embedding tables

Usage: python verify_governance_wiring.py <db_path>
"""

import sqlite3
import sys
from datetime import datetime


def test_governance_query_audit(conn):
    """Test governance query audit logging."""
    print("\n=== Testing governance_query_audit ===")
    
    cursor = conn.cursor()
    
    # Check table exists
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='governance_query_audit'")
    if not cursor.fetchone():
        print("ERROR: governance_query_audit table not found")
        return False
    
    # Check view exists
    cursor.execute("SELECT name FROM sqlite_master WHERE type='view' AND name='v_neurorights_query_stats'")
    if not cursor.fetchone():
        print("ERROR: v_neurorights_query_stats view not found")
        return False
    
    # Insert a test row
    cursor.execute("""
        INSERT INTO governance_query_audit (tool_name, caller_id, ker_k, ker_e, ker_r, ker_s, neuro_flag, lane_default, query_payload)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, ('test_tool', 'test_caller', 0.8, 0.7, 0.3, 0.26, 0, 'EXPPROD', '{"test": true}'))
    
    # Verify insert
    cursor.execute("SELECT * FROM governance_query_audit ORDER BY id DESC LIMIT 1")
    row = cursor.fetchone()
    if row:
        print(f"  ✓ Audit row inserted: tool={row[1]}, neuro_flag={row[6]}, lane={row[7]}")
    else:
        print("  ✗ Failed to verify audit row")
        return False
    
    # Test view
    cursor.execute("SELECT * FROM v_neurorights_query_stats ORDER BY day DESC LIMIT 3")
    stats = cursor.fetchall()
    if stats:
        print(f"  ✓ View working: {len(stats)} days of stats")
        for s in stats:
            print(f"    Day {s[0]}: total={s[1]}, neuro={s[2]}, neuro_prod={s[3]}")
    
    return True


def test_actuation_request(conn):
    """Test actuation request table and corridor trigger."""
    print("\n=== Testing actuation_request ===")
    
    cursor = conn.cursor()
    
    # Check table exists
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='actuation_request'")
    if not cursor.fetchone():
        print("ERROR: actuation_request table not found")
        return False
    
    # Check trigger exists
    cursor.execute("SELECT name FROM sqlite_master WHERE type='trigger' AND name='actuation_request_corridor_enforce'")
    if not cursor.fetchone():
        print("ERROR: actuation_request_corridor_enforce trigger not found")
        return False
    
    # Test successful insert (corridor_ok=1)
    try:
        cursor.execute("""
            INSERT INTO actuation_request (hex_id, dvt_pred, vt_before, vt_after, corridor_ok)
            VALUES (?, ?, ?, ?, 1)
        """, ('hex_test_001', 0.005, 0.02, 0.025))
        print("  ✓ Safe actuation logged successfully")
    except sqlite3.Error as e:
        print(f"  ✗ Failed to insert safe actuation: {e}")
        return False
    
    # Test blocked insert (corridor_ok=0 should trigger abort)
    try:
        cursor.execute("""
            INSERT INTO actuation_request (hex_id, dvt_pred, vt_before, vt_after, corridor_ok)
            VALUES (?, ?, ?, ?, 0)
        """, ('hex_test_002', 0.05, 0.02, 0.07))
        print("  ✗ Trigger did not block unsafe actuation!")
        return False
    except sqlite3.Error as e:
        if "Lyapunov-KER corridor violation" in str(e):
            print("  ✓ Unsafe actuation correctly blocked by trigger")
        else:
            print(f"  ? Unexpected error: {e}")
    
    return True


def test_knowledge_graph(conn):
    """Test knowledge graph schema."""
    print("\n=== Testing knowledge graph ===")
    
    cursor = conn.cursor()
    
    # Check tables exist
    for table in ['kg_node', 'kg_edge', 'kg_embedding']:
        cursor.execute(f"SELECT name FROM sqlite_master WHERE type='table' AND name='{table}'")
        if not cursor.fetchone():
            print(f"ERROR: {table} table not found")
            return False
        print(f"  ✓ {table} table exists")
    
    # Insert test nodes
    cursor.execute("INSERT OR REPLACE INTO kg_node (node_id, node_type, ker_k, ker_e, ker_r, ker_s) VALUES (?, ?, ?, ?, ?, ?)",
                   ('Module:test_module', 'MODULE', 0.8, 0.7, 0.3, 0.26))
    cursor.execute("INSERT OR REPLACE INTO kg_node (node_id, node_type) VALUES (?, ?)",
                   ('Lane:PROD', 'LANE'))
    
    # Insert test edge
    cursor.execute("INSERT OR IGNORE INTO kg_edge (src_id, dst_id, relation) VALUES (?, ?, ?)",
                   ('Module:test_module', 'Lane:PROD', 'LANE_OF'))
    
    # Insert test embedding
    vec_str = ','.join(str(i * 0.01) for i in range(32))
    cursor.execute("INSERT OR REPLACE INTO kg_embedding (node_id, dim, vector) VALUES (?, ?, ?)",
                   ('Module:test_module', 32, vec_str))
    
    # Verify
    cursor.execute("SELECT COUNT(*) FROM kg_node")
    node_count = cursor.fetchone()[0]
    cursor.execute("SELECT COUNT(*) FROM kg_edge")
    edge_count = cursor.fetchone()[0]
    cursor.execute("SELECT COUNT(*) FROM kg_embedding")
    emb_count = cursor.fetchone()[0]
    
    print(f"  ✓ KG populated: {node_count} nodes, {edge_count} edges, {emb_count} embeddings")
    
    return True


def main():
    if len(sys.argv) < 2:
        print("Usage: python verify_governance_wiring.py <db_path>")
        return 1
    
    db_path = sys.argv[1]
    conn = sqlite3.connect(db_path)
    
    all_passed = True
    
    # Run SQL init scripts first
    print("Initializing governance schemas...")
    with open('/workspace/sql/governance_query_audit.sql', 'r') as f:
        conn.executescript(f.read())
    with open('/workspace/sql/actuator_audit_schema.sql', 'r') as f:
        conn.executescript(f.read())
    with open('/workspace/sql/governance_knowledge_graph.sql', 'r') as f:
        conn.executescript(f.read())
    conn.commit()
    print("Schemas initialized.")
    
    # Run tests
    all_passed &= test_governance_query_audit(conn)
    all_passed &= test_actuation_request(conn)
    all_passed &= test_knowledge_graph(conn)
    
    conn.close()
    
    print("\n" + "="*50)
    if all_passed:
        print("✓ All governance wiring tests PASSED")
        return 0
    else:
        print("✗ Some governance wiring tests FAILED")
        return 1


if __name__ == '__main__':
    sys.exit(main())
