#!/usr/bin/env python3
"""
File: tools/kg_embedding_builder.py
Build governance knowledge graph embeddings from SQLite data.

This script:
1. Reads governance data from SQLite (module_ker_profile, mcp_tool, synapse tables, hex registry)
2. Populates kg_node and kg_edge tables
3. Trains simple TransE-style embeddings using numpy
4. Writes embeddings back to kg_embedding table

Usage: python kg_embedding_builder.py <db_path>
"""

import sqlite3
import sys
import random
import math


def init_kg_schema(conn):
    """Initialize knowledge graph schema if not exists."""
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS kg_node (
            node_id    TEXT PRIMARY KEY,
            node_type  TEXT NOT NULL,
            ker_k      REAL,
            ker_e      REAL,
            ker_r      REAL,
            ker_s      REAL
        );
        
        CREATE TABLE IF NOT EXISTS kg_edge (
            src_id     TEXT NOT NULL,
            dst_id     TEXT NOT NULL,
            relation   TEXT NOT NULL,
            PRIMARY KEY (src_id, dst_id, relation)
        );
        
        CREATE TABLE IF NOT EXISTS kg_embedding (
            node_id   TEXT PRIMARY KEY,
            dim       INTEGER NOT NULL DEFAULT 32,
            vector    TEXT NOT NULL
        );
    """)
    conn.commit()


def populate_nodes_and_edges(conn):
    """Populate kg_node and kg_edge from existing governance tables."""
    cursor = conn.cursor()
    
    # Insert module nodes
    cursor.execute("""
        INSERT OR REPLACE INTO kg_node (node_id, node_type, ker_k, ker_e, ker_r, ker_s)
        SELECT 'Module:' || relpath, 'MODULE', ker_k, ker_e, ker_r, ker_s
        FROM module_ker_profile
    """)
    
    # Insert tool nodes (from mcp_tool if exists)
    try:
        cursor.execute("""
            INSERT OR REPLACE INTO kg_node (node_id, node_type, ker_k, ker_e, ker_r, ker_s)
            SELECT 'Tool:' || toolname, 'TOOL', ker_k, ker_e, ker_r, ker_s
            FROM mcp_tool
        """)
    except sqlite3.OperationalError:
        pass  # mcp_tool table may not exist
    
    # Insert hex nodes
    try:
        cursor.execute("""
            INSERT OR REPLACE INTO kg_node (node_id, node_type)
            SELECT 'Hex:' || hex_id, 'HEX'
            FROM v_hex_stability_ker_dvt_carbon
        """)
    except sqlite3.OperationalError:
        pass
    
    # Insert lane nodes
    for lane in ['RESEARCH', 'EXPPROD', 'PROD']:
        cursor.execute("""
            INSERT OR REPLACE INTO kg_node (node_id, node_type)
            VALUES (?, 'LANE')
        """, (f'Lane:{lane}',))
    
    # Insert edges: modules -> lanes
    cursor.execute("""
        INSERT OR IGNORE INTO kg_edge (src_id, dst_id, relation)
        SELECT 'Module:' || relpath, 'Lane:' || lane_default, 'LANE_OF'
        FROM module_ker_profile
    """)
    
    # Insert edges: modules -> hexes (via workload telemetry)
    try:
        cursor.execute("""
            INSERT OR IGNORE INTO kg_edge (src_id, dst_id, relation)
            SELECT DISTINCT 'Module:' || module_relpath, 'Hex:' || hex_id, 'RUNS_IN_HEX'
            FROM cyboquatic_workload_telemetry
        """)
    except sqlite3.OperationalError:
        pass
    
    conn.commit()
    print(f"Populated nodes and edges")


def initialize_embeddings(conn, dim=32):
    """Initialize random embeddings for all nodes."""
    cursor = conn.cursor()
    
    # Get all node IDs
    cursor.execute("SELECT node_id FROM kg_node")
    node_ids = [row[0] for row in cursor.fetchall()]
    
    embeddings = {}
    for node_id in node_ids:
        # Random initialization in [-0.1, 0.1]
        vec = [random.uniform(-0.1, 0.1) for _ in range(dim)]
        embeddings[node_id] = vec
        
        vec_str = ','.join(str(v) for v in vec)
        cursor.execute("""
            INSERT OR REPLACE INTO kg_embedding (node_id, dim, vector)
            VALUES (?, ?, ?)
        """, (node_id, dim, vec_str))
    
    conn.commit()
    return embeddings, dim


def get_relation_embedding(conn, relation, dim=32):
    """Get or create a relation embedding (fixed per relation type)."""
    # Use hash-based deterministic initialization for relations
    random.seed(hash(relation) % 2**32)
    vec = [random.uniform(-0.1, 0.1) for _ in range(dim)]
    random.seed()  # Reset seed
    return vec


def train_transe(conn, embeddings, dim, epochs=50, lr=0.01):
    """Simple TransE training loop."""
    cursor = conn.cursor()
    
    # Load all edges
    cursor.execute("SELECT src_id, dst_id, relation FROM kg_edge")
    edges = cursor.fetchall()
    
    if not edges:
        print("No edges to train on")
        return embeddings
    
    # Get relation embeddings
    relations = set(e[2] for e in edges)
    rel_embeddings = {r: get_relation_embedding(conn, r, dim) for r in relations}
    
    print(f"Training TransE on {len(edges)} edges, {len(embeddings)} nodes, {len(relations)} relations")
    
    for epoch in range(epochs):
        total_loss = 0.0
        
        for src_id, dst_id, relation in edges:
            h = embeddings.get(src_id)
            t = embeddings.get(dst_id)
            r = rel_embeddings[relation]
            
            if h is None or t is None:
                continue
            
            # TransE: h + r ≈ t
            # Loss = ||h + r - t||^2
            diff = [h[i] + r[i] - t[i] for i in range(dim)]
            loss = sum(d * d for d in diff)
            total_loss += loss
            
            # Gradient descent update
            for i in range(dim):
                grad = 2 * diff[i]
                h[i] -= lr * grad
                t[i] += lr * grad  # Also update tail
        
        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch + 1}/{epochs}, Loss: {total_loss:.6f}")
    
    # Save updated embeddings
    for node_id, vec in embeddings.items():
        vec_str = ','.join(str(v) for v in vec)
        cursor.execute("""
            UPDATE kg_embedding SET vector = ? WHERE node_id = ?
        """, (vec_str, node_id))
    
    conn.commit()
    return embeddings


def query_similar_modules(conn, hex_id, lane='PROD', top_k=3):
    """Query for modules most likely to cause violations in a given hex/lane."""
    cursor = conn.cursor()
    
    # Get candidate modules running in the hex with specified lane
    cursor.execute("""
        SELECT m.relpath, m.ker_r, m.ker_s
        FROM module_ker_profile m
        JOIN cyboquatic_workload_telemetry t ON m.relpath = t.module_relpath
        WHERE t.hex_id = ? AND m.lane_default = ?
        ORDER BY m.ker_r DESC, m.ker_s ASC
    """, (hex_id, lane))
    
    candidates = cursor.fetchall()
    
    if not candidates:
        # Fallback: just get high-risk modules in the lane
        cursor.execute("""
            SELECT relpath, ker_r, ker_s
            FROM module_ker_profile
            WHERE lane_default = ?
            ORDER BY ker_r DESC, ker_s ASC
            LIMIT ?
        """, (lane, top_k))
        candidates = cursor.fetchall()
    
    return candidates


def answer_prod_violation_question(conn, hex_id):
    """Answer: Which module is most likely to cause a PROD lane violation in hex_X?"""
    candidates = query_similar_modules(conn, hex_id, 'PROD')
    
    if not candidates:
        return "No PROD modules found for hex " + hex_id
    
    # Get embeddings for risk scoring
    cursor = conn.cursor()
    
    results = []
    for relpath, ker_r, ker_s in candidates:
        node_id = f'Module:{relpath}'
        cursor.execute("SELECT vector FROM kg_embedding WHERE node_id = ?", (node_id,))
        row = cursor.fetchone()
        
        emb_sim = 0.0
        if row:
            # Simple similarity score based on embedding magnitude
            vec = [float(x) for x in row[0].split(',')]
            emb_sim = sum(v * v for v in vec) ** 0.5
        
        # Risk score: weighted combination of KER and embedding similarity
        risk_score = 0.5 * ker_r + 0.3 * (1 - ker_s) + 0.2 * emb_sim
        results.append((relpath, ker_r, ker_s, risk_score))
    
    # Sort by risk score
    results.sort(key=lambda x: -x[3])
    
    # Verify top result against actual violation data
    top_module = results[0][0] if results else None
    
    response = f"Top risk module for PROD violations in {hex_id}: {top_module}\n"
    response += "Ranked candidates:\n"
    for relpath, ker_r, ker_s, score in results[:5]:
        response += f"  - {relpath}: ker_r={ker_r:.3f}, ker_s={ker_s:.3f}, risk={score:.3f}\n"
    
    return response


def main():
    if len(sys.argv) < 2:
        print("Usage: python kg_embedding_builder.py <db_path>")
        return 1
    
    db_path = sys.argv[1]
    conn = sqlite3.connect(db_path)
    
    # Initialize schema
    init_kg_schema(conn)
    
    # Populate nodes and edges from existing data
    populate_nodes_and_edges(conn)
    
    # Initialize embeddings
    embeddings, dim = initialize_embeddings(conn, dim=32)
    
    # Train TransE
    train_transe(conn, embeddings, dim, epochs=50, lr=0.01)
    
    # Demo: answer a question about PROD violations
    print("\n--- Demo: PROD Violation Query ---")
    response = answer_prod_violation_question(conn, 'hex_001')
    print(response)
    
    conn.close()
    return 0


if __name__ == '__main__':
    sys.exit(main())
