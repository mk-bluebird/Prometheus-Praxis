-- File: sql/governance_knowledge_graph.sql
-- Knowledge graph schema for governance entities and embeddings

PRAGMA foreign_keys = ON;

-- Knowledge graph nodes (modules, tools, hexes, lanes, violations)
CREATE TABLE IF NOT EXISTS kg_node (
    node_id    TEXT PRIMARY KEY,
    node_type  TEXT NOT NULL,  -- MODULE, TOOL, HEX, LANE, VIOLATION
    ker_k      REAL,
    ker_e      REAL,
    ker_r      REAL,
    ker_s      REAL
);

CREATE INDEX IF NOT EXISTS idx_kg_node_type ON kg_node(node_type);

-- Knowledge graph edges (relationships between nodes)
CREATE TABLE IF NOT EXISTS kg_edge (
    src_id     TEXT NOT NULL,
    dst_id     TEXT NOT NULL,
    relation   TEXT NOT NULL,  -- DEPENDS_ON, RUNS_IN_HEX, HAS_VIOLATION, LANE_OF
    PRIMARY KEY (src_id, dst_id, relation)
);

CREATE INDEX IF NOT EXISTS idx_kg_edge_src ON kg_edge(src_id);
CREATE INDEX IF NOT EXISTS idx_kg_edge_dst ON kg_edge(dst_id);
CREATE INDEX IF NOT EXISTS idx_kg_edge_relation ON kg_edge(relation);

-- Embeddings table for TransE-style vectors
CREATE TABLE IF NOT EXISTS kg_embedding (
    node_id   TEXT PRIMARY KEY,
    dim       INTEGER NOT NULL DEFAULT 32,
    vector    TEXT NOT NULL  -- comma-separated floats
);

CREATE INDEX IF NOT EXISTS idx_kg_embedding_node ON kg_embedding(node_id);
