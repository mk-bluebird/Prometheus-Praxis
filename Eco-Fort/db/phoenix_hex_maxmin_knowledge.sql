-- filename: Eco-Fort/db/phoenix_hex_maxmin_knowledge.sql
-- destination: Eco-Fort/db/phoenix_hex_maxmin_knowledge.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
--
-- Purpose:
--   SQL (SQLite) implementation of a max-min knowledge flow problem
--   over a directed acyclic graph (DAG) of Phoenix hex anchors:
--     - Nodes: phoenixhexanchor (hex anchors).
--     - Edges: phoenixhexedge (directed links) carrying K,E,R.
--   Objective:
--     Maximise the minimum knowledge factor K along any path from a
--     RESEARCH shard (source) to a PROD canal (sink), using a recursive
--     CTE to propagate path-minimum K values.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Edge table with K,E,R per link
----------------------------------------------------------------------

-- Assumes an existing phoenixhexanchor table; we add an edge table.
CREATE TABLE IF NOT EXISTS phoenixhexedge (
    edge_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    from_hex      TEXT    NOT NULL,
    to_hex        TEXT    NOT NULL,
    k_edge        REAL    NOT NULL,
    e_edge        REAL    NOT NULL,
    r_edge        REAL    NOT NULL,
    evidence_hex  TEXT    NOT NULL,
    created_utc   TEXT    NOT NULL,
    FOREIGN KEY (from_hex) REFERENCES phoenixhexanchor(evidencehex)
        ON DELETE RESTRICT,
    FOREIGN KEY (to_hex)   REFERENCES phoenixhexanchor(evidencehex)
        ON DELETE RESTRICT,
    CHECK (k_edge BETWEEN 0.0 AND 1.0),
    CHECK (e_edge BETWEEN 0.0 AND 1.0),
    CHECK (r_edge BETWEEN 0.0 AND 1.0)
);

CREATE INDEX IF NOT EXISTS idx_phxedge_from_to
    ON phoenixhexedge (from_hex, to_hex);

----------------------------------------------------------------------
-- 2. Recursive CTE for max-min K flow
----------------------------------------------------------------------

-- We want to compute, for each path from a source (RESEARCH shard)
-- to a sink (PROD canal), the minimum K along that path, and pick
-- the path with the maximum such minimum.

-- Assume:
--   - Sources are anchors with domain='CYBOQUATIC' and subdomain='RESEARCH'
--   - Sinks are anchors with domain='CYBOQUATIC' and subdomain='PROD_CANAL'
--   and that phoenixhexanchor has fields domain, subdomain, evidencehex.

WITH RECURSIVE
    -- Start paths from RESEARCH shards.
    seed_paths AS (
        SELECT
            a.evidencehex      AS current_hex,
            a.evidencehex      AS path_start_hex,
            a.evidencehex      AS path_sequence,
            1                  AS depth,
            1.0                AS min_k -- initial K bottleneck (no edge yet)
        FROM phoenixhexanchor AS a
        WHERE a.domain   = 'CYBOQUATIC'
          AND a.subdomain = 'RESEARCH'
    ),
    -- Extend paths along edges, updating the path-minimum K.
    path_flow AS (
        -- Base from seeds.
        SELECT
            s.current_hex,
            s.path_start_hex,
            s.path_sequence,
            s.depth,
            s.min_k
        FROM seed_paths AS s

        UNION ALL

        -- Extend along outgoing edges.
        SELECT
            e.to_hex           AS current_hex,
            p.path_start_hex   AS path_start_hex,
            p.path_sequence || '->' || e.to_hex AS path_sequence,
            p.depth + 1        AS depth,
            MIN(p.min_k, e.k_edge) AS min_k  -- path bottleneck K
        FROM path_flow AS p
        JOIN phoenixhexedge AS e
          ON e.from_hex = p.current_hex
    )

-- Select only paths that reach PROD canals, and pick the one(s)
-- with maximum bottleneck K.
SELECT
    path_start_hex,
    current_hex   AS path_end_hex,
    path_sequence,
    depth,
    min_k
FROM path_flow
JOIN phoenixhexanchor AS sink
  ON sink.evidencehex = path_flow.current_hex
WHERE sink.domain   = 'CYBOQUATIC'
  AND sink.subdomain = 'PROD_CANAL'
ORDER BY min_k DESC, depth ASC
LIMIT 1;
