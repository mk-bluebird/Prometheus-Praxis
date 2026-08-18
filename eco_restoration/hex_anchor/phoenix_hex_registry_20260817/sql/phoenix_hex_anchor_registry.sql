PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS phoenix_hex_grid_definitions (
    hex_grid_definition_id INTEGER PRIMARY KEY,
    grid_version TEXT NOT NULL UNIQUE,
    origin_latitude_deg REAL NOT NULL CHECK (origin_latitude_deg BETWEEN -90.0 AND 90.0),
    origin_longitude_deg REAL NOT NULL CHECK (origin_longitude_deg BETWEEN -180.0 AND 180.0),
    meters_per_degree_latitude REAL NOT NULL CHECK (meters_per_degree_latitude > 0.0),
    meters_per_degree_longitude REAL NOT NULL CHECK (meters_per_degree_longitude > 0.0),
    hex_side_length_m REAL NOT NULL CHECK (hex_side_length_m > 0.0),
    coordinate_method TEXT NOT NULL CHECK (
        coordinate_method IN ('DECLARED_LOCAL_PLANAR_APPROXIMATION', 'APPROVED_PROJECTED_CRS')
    ),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    created_at_utc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS phoenix_hex_anchors (
    phoenix_hex_anchor_id INTEGER PRIMARY KEY,
    hex_grid_definition_id INTEGER NOT NULL REFERENCES phoenix_hex_grid_definitions(hex_grid_definition_id),
    anchor_code TEXT NOT NULL UNIQUE,
    q INTEGER NOT NULL,
    r INTEGER NOT NULL,
    s INTEGER NOT NULL,
    center_x_m REAL NOT NULL,
    center_y_m REAL NOT NULL,
    center_latitude_deg REAL NOT NULL CHECK (center_latitude_deg BETWEEN -90.0 AND 90.0),
    center_longitude_deg REAL NOT NULL CHECK (center_longitude_deg BETWEEN -180.0 AND 180.0),
    created_at_utc TEXT NOT NULL,
    CHECK (s = -q - r),
    UNIQUE (hex_grid_definition_id, q, r, s)
);

CREATE TABLE IF NOT EXISTS canal_nodes (
    canal_node_id TEXT PRIMARY KEY NOT NULL,
    node_name TEXT NOT NULL UNIQUE,
    phoenix_hex_anchor_id INTEGER NOT NULL REFERENCES phoenix_hex_anchors(phoenix_hex_anchor_id),
    latitude_deg REAL NOT NULL CHECK (latitude_deg BETWEEN -90.0 AND 90.0),
    longitude_deg REAL NOT NULL CHECK (longitude_deg BETWEEN -180.0 AND 180.0),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    registered_at_utc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS hex_anchor_neighbors (
    phoenix_hex_anchor_id INTEGER NOT NULL REFERENCES phoenix_hex_anchors(phoenix_hex_anchor_id) ON DELETE CASCADE,
    neighbor_anchor_id INTEGER NOT NULL REFERENCES phoenix_hex_anchors(phoenix_hex_anchor_id) ON DELETE CASCADE,
    direction_code TEXT NOT NULL CHECK (
        direction_code IN ('E', 'NE', 'NW', 'W', 'SW', 'SE')
    ),
    PRIMARY KEY (phoenix_hex_anchor_id, neighbor_anchor_id),
    UNIQUE (phoenix_hex_anchor_id, direction_code),
    CHECK (phoenix_hex_anchor_id <> neighbor_anchor_id)
);

CREATE INDEX IF NOT EXISTS idx_hex_anchor_grid_qrs
    ON phoenix_hex_anchors(
        hex_grid_definition_id,
        q,
        r,
        s,
        phoenix_hex_anchor_id,
        anchor_code
    );

CREATE INDEX IF NOT EXISTS idx_canal_node_anchor_active
    ON canal_nodes(
        phoenix_hex_anchor_id,
        active,
        canal_node_id,
        node_name
    );

CREATE INDEX IF NOT EXISTS idx_hex_neighbor_anchor_direction
    ON hex_anchor_neighbors(
        phoenix_hex_anchor_id,
        direction_code,
        neighbor_anchor_id
    );

CREATE TRIGGER IF NOT EXISTS trg_anchor_center_matches_cube_coordinates
BEFORE INSERT ON phoenix_hex_anchors
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.s <> -NEW.q - NEW.r
        THEN RAISE(ABORT, 'hex anchor violates s = -q-r')
        WHEN ABS(
            NEW.center_x_m - (
                SELECT hex_side_length_m * SQRT(3.0) * (NEW.q + NEW.r / 2.0)
                FROM phoenix_hex_grid_definitions
                WHERE hex_grid_definition_id = NEW.hex_grid_definition_id
            )
        ) > 0.000001
        THEN RAISE(ABORT, 'center_x_m inconsistent with pointy-top cube coordinates')
        WHEN ABS(
            NEW.center_y_m - (
                SELECT hex_side_length_m * 1.5 * NEW.r
                FROM phoenix_hex_grid_definitions
                WHERE hex_grid_definition_id = NEW.hex_grid_definition_id
            )
        ) > 0.000001
        THEN RAISE(ABORT, 'center_y_m inconsistent with pointy-top cube coordinates')
    END;
END;

CREATE TRIGGER IF NOT EXISTS trg_neighbor_cube_adjacency
BEFORE INSERT ON hex_anchor_neighbors
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NOT EXISTS (
            SELECT 1
            FROM phoenix_hex_anchors AS source_anchor
            JOIN phoenix_hex_anchors AS neighbor_anchor
                ON neighbor_anchor.phoenix_hex_anchor_id = NEW.neighbor_anchor_id
            WHERE source_anchor.phoenix_hex_anchor_id = NEW.phoenix_hex_anchor_id
              AND source_anchor.hex_grid_definition_id = neighbor_anchor.hex_grid_definition_id
              AND ABS(neighbor_anchor.q - source_anchor.q) <= 1
              AND ABS(neighbor_anchor.r - source_anchor.r) <= 1
              AND ABS(neighbor_anchor.s - source_anchor.s) <= 1
              AND ABS(neighbor_anchor.q - source_anchor.q)
                  + ABS(neighbor_anchor.r - source_anchor.r)
                  + ABS(neighbor_anchor.s - source_anchor.s) = 2
        )
        THEN RAISE(ABORT, 'hex neighbor must be an adjacent cube-coordinate anchor in the same grid')
    END;
END;

CREATE VIEW IF NOT EXISTS v_phoenix_canal_node_hex_lookup AS
SELECT
    node.canal_node_id,
    node.node_name,
    node.latitude_deg,
    node.longitude_deg,
    node.active,
    grid.grid_version,
    grid.hex_side_length_m,
    anchor.anchor_code,
    anchor.q,
    anchor.r,
    anchor.s,
    anchor.center_x_m,
    anchor.center_y_m,
    anchor.center_latitude_deg,
    anchor.center_longitude_deg
FROM canal_nodes AS node
JOIN phoenix_hex_anchors AS anchor
    ON anchor.phoenix_hex_anchor_id = node.phoenix_hex_anchor_id
JOIN phoenix_hex_grid_definitions AS grid
    ON grid.hex_grid_definition_id = anchor.hex_grid_definition_id;

CREATE VIEW IF NOT EXISTS v_phoenix_hex_neighbor_lookup AS
SELECT
    source.anchor_code AS source_anchor_code,
    source.q AS source_q,
    source.r AS source_r,
    source.s AS source_s,
    neighbor_relation.direction_code,
    neighbor.anchor_code AS neighbor_anchor_code,
    neighbor.q AS neighbor_q,
    neighbor.r AS neighbor_r,
    neighbor.s AS neighbor_s
FROM hex_anchor_neighbors AS neighbor_relation
JOIN phoenix_hex_anchors AS source
    ON source.phoenix_hex_anchor_id = neighbor_relation.phoenix_hex_anchor_id
JOIN phoenix_hex_anchors AS neighbor
    ON neighbor.phoenix_hex_anchor_id = neighbor_relation.neighbor_anchor_id;
