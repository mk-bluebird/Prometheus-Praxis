PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS phoenix_hex_grid_definitions (
    hex_grid_definition_id INTEGER PRIMARY KEY,
    grid_version TEXT NOT NULL UNIQUE,
    coordinate_offset INTEGER NOT NULL CHECK (coordinate_offset >= 0),
    linear_radix INTEGER NOT NULL CHECK (
        linear_radix > coordinate_offset * 2
    ),
    hex_side_length_m REAL NOT NULL CHECK (hex_side_length_m > 0.0),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    created_at_utc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS phoenix_hex_nodes (
    phoenix_hex_node_id INTEGER PRIMARY KEY,
    hex_grid_definition_id INTEGER NOT NULL REFERENCES phoenix_hex_grid_definitions(hex_grid_definition_id),
    linear_anchor_id INTEGER NOT NULL CHECK (linear_anchor_id >= 0),
    q INTEGER NOT NULL,
    r INTEGER NOT NULL,
    s INTEGER NOT NULL,
    corridor_name TEXT NOT NULL,
    passable INTEGER NOT NULL DEFAULT 1 CHECK (passable IN (0, 1)),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    CHECK (s = -q - r),
    UNIQUE (hex_grid_definition_id, q, r, s),
    UNIQUE (hex_grid_definition_id, linear_anchor_id)
);

CREATE TABLE IF NOT EXISTS phoenix_hex_edges (
    from_hex_node_id INTEGER NOT NULL REFERENCES phoenix_hex_nodes(phoenix_hex_node_id) ON DELETE CASCADE,
    to_hex_node_id INTEGER NOT NULL REFERENCES phoenix_hex_nodes(phoenix_hex_node_id) ON DELETE CASCADE,
    direction_code TEXT NOT NULL CHECK (
        direction_code IN ('E', 'W', 'NE', 'NW', 'SE', 'SW')
    ),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    PRIMARY KEY (from_hex_node_id, to_hex_node_id),
    CHECK (from_hex_node_id <> to_hex_node_id)
);

CREATE INDEX IF NOT EXISTS idx_hex_nodes_grid_qr_passable
    ON phoenix_hex_nodes(
        hex_grid_definition_id,
        q,
        r,
        passable,
        active,
        phoenix_hex_node_id,
        linear_anchor_id
    );

CREATE INDEX IF NOT EXISTS idx_hex_nodes_corridor_active
    ON phoenix_hex_nodes(
        corridor_name,
        active,
        passable,
        phoenix_hex_node_id
    );

CREATE INDEX IF NOT EXISTS idx_hex_edges_from_active
    ON phoenix_hex_edges(
        from_hex_node_id,
        active,
        to_hex_node_id,
        direction_code
    );

CREATE TRIGGER IF NOT EXISTS trg_hex_node_linear_id_consistency
BEFORE INSERT ON phoenix_hex_nodes
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.s <> -NEW.q - NEW.r
        THEN RAISE(ABORT, 'hex node violates s = -q-r')
        WHEN NEW.q < -(
            SELECT coordinate_offset
            FROM phoenix_hex_grid_definitions
            WHERE hex_grid_definition_id = NEW.hex_grid_definition_id
        )
        OR NEW.q > (
            SELECT coordinate_offset
            FROM phoenix_hex_grid_definitions
            WHERE hex_grid_definition_id = NEW.hex_grid_definition_id
        )
        OR NEW.r < -(
            SELECT coordinate_offset
            FROM phoenix_hex_grid_definitions
            WHERE hex_grid_definition_id = NEW.hex_grid_definition_id
        )
        OR NEW.r > (
            SELECT coordinate_offset
            FROM phoenix_hex_hex_grid_definitions
            WHERE hex_grid_definition_id = NEW.hex_grid_definition_id
        )
        THEN RAISE(ABORT, 'q/r coordinates exceed declared reversible range')
        WHEN NEW.linear_anchor_id <> (
            (NEW.q + (
                SELECT coordinate_offset
                FROM phoenix_hex_grid_definitions
                WHERE hex_grid_definition_id = NEW.hex_grid_definition_id
            ))
            + (
                SELECT linear_radix
                FROM phoenix_hex_grid_definitions
                WHERE hex_grid_definition_id = NEW.hex_grid_definition_id
            ) * (
                NEW.r + (
                    SELECT coordinate_offset
                    FROM phoenix_hex_grid_definitions
                    WHERE hex_grid_definition_id = NEW.hex_grid_definition_id
                )
            )
        )
        THEN RAISE(ABORT, 'linear anchor id is inconsistent with declared reversible encoding')
    END;
END;

CREATE TRIGGER IF NOT EXISTS trg_hex_edge_is_six_neighbor
BEFORE INSERT ON phoenix_hex_edges
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NOT EXISTS (
            SELECT 1
            FROM phoenix_hex_nodes AS source_node
            JOIN phoenix_hex_nodes AS target_node
                ON target_node.phoenix_hex_node_id = NEW.to_hex_node_id
            WHERE source_node.phoenix_hex_node_id = NEW.from_hex_node_id
              AND source_node.hex_grid_definition_id = target_node.hex_grid_definition_id
              AND (
                  (target_node.q = source_node.q + 1 AND target_node.r = source_node.r)
                  OR
                  (target_node.q = source_node.q - 1 AND target_node.r = source_node.r)
                  OR
                  (target_node.q = source_node.q AND target_node.r = source_node.r + 1)
                  OR
                  (target_node.q = source_node.q AND target_node.r = source_node.r - 1)
                  OR
                  (target_node.q = source_node.q + 1 AND target_node.r = source_node.r - 1)
                  OR
                  (target_node.q = source_node.q - 1 AND target_node.r = source_node.r + 1)
              )
        )
        THEN RAISE(ABORT, 'edge is not a valid six-neighbor cube-coordinate adjacency')
    END;
END;

CREATE VIEW IF NOT EXISTS v_active_hex_corridor_graph AS
SELECT
    source.hex_grid_definition_id,
    source.linear_anchor_id AS from_anchor_id,
    source.q AS from_q,
    source.r AS from_r,
    source.s AS from_s,
    source.corridor_name AS from_corridor,
    target.linear_anchor_id AS to_anchor_id,
    target.q AS to_q,
    target.r AS to_r,
    target.s AS to_s,
    target.corridor_name AS to_corridor,
    edge.direction_code
FROM phoenix_hex_edges AS edge
JOIN phoenix_hex_nodes AS source
    ON source.phoenix_hex_node_id = edge.from_hex_node_id
JOIN phoenix_hex_nodes AS target
    ON target.phoenix_hex_node_id = edge.to_hex_node_id
WHERE edge.active = 1
  AND source.active = 1
  AND target.active = 1
  AND source.passable = 1
  AND target.passable = 1;
