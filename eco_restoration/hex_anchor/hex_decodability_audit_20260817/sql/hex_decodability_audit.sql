PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS phoenix_hex_grid_definitions (
    hex_grid_definition_id INTEGER PRIMARY KEY,
    grid_version TEXT NOT NULL UNIQUE,
    coordinate_offset INTEGER NOT NULL CHECK (coordinate_offset >= 0),
    linear_radix INTEGER NOT NULL CHECK (linear_radix > coordinate_offset * 2),
    coordinate_method TEXT NOT NULL CHECK (
        coordinate_method IN ('DECLARED_LOCAL_PLANAR_APPROXIMATION', 'APPROVED_PROJECTED_CRS')
    ),
    created_at_utc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS phoenix_hex_anchor_registry (
    phoenix_hex_anchor_id INTEGER PRIMARY KEY,
    hex_grid_definition_id INTEGER NOT NULL REFERENCES phoenix_hex_grid_definitions(hex_grid_definition_id),
    anchor_code TEXT NOT NULL UNIQUE,
    q INTEGER NOT NULL,
    r INTEGER NOT NULL,
    s INTEGER NOT NULL,
    linear_anchor_id INTEGER NOT NULL CHECK (linear_anchor_id >= 0),
    original_latitude_deg REAL NOT NULL CHECK (original_latitude_deg BETWEEN -90.0 AND 90.0),
    original_longitude_deg REAL NOT NULL CHECK (original_longitude_deg BETWEEN -180.0 AND 180.0),
    center_latitude_deg REAL NOT NULL CHECK (center_latitude_deg BETWEEN -90.0 AND 90.0),
    center_longitude_deg REAL NOT NULL CHECK (center_longitude_deg BETWEEN -180.0 AND 180.0),
    transform_method TEXT NOT NULL CHECK (
        transform_method = 'LINEAR_OFFSET_RADIX_CUBE_COORDINATES'
    ),
    created_at_utc TEXT NOT NULL,
    CHECK (s = -q - r),
    UNIQUE (hex_grid_definition_id, q, r, s),
    UNIQUE (hex_grid_definition_id, linear_anchor_id)
);

CREATE INDEX IF NOT EXISTS idx_hex_registry_grid_linear_id
    ON phoenix_hex_anchor_registry(
        hex_grid_definition_id,
        linear_anchor_id,
        q,
        r,
        s,
        anchor_code
    );

CREATE INDEX IF NOT EXISTS idx_hex_registry_grid_cube
    ON phoenix_hex_anchor_registry(
        hex_grid_definition_id,
        q,
        r,
        s,
        linear_anchor_id
    );

CREATE TRIGGER IF NOT EXISTS trg_hex_anchor_bijection
BEFORE INSERT ON phoenix_hex_anchor_registry
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.s <> -NEW.q - NEW.r
        THEN RAISE(ABORT, 'cube-coordinate constraint s=-q-r failed')
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
            FROM phoenix_hex_grid_definitions
            WHERE hex_grid_definition_id = NEW.hex_grid_definition_id
        )
        THEN RAISE(ABORT, 'q/r outside declared reversible coordinate window')
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
        THEN RAISE(ABORT, 'linear anchor id does not equal reversible offset-radix encoding')
    END;
END;

CREATE VIEW IF NOT EXISTS v_hex_anchor_inverse_audit AS
SELECT
    registry.anchor_code,
    grid.grid_version,
    registry.linear_anchor_id,
    registry.q AS stored_q,
    registry.r AS stored_r,
    registry.s AS stored_s,
    (registry.linear_anchor_id % grid.linear_radix) - grid.coordinate_offset AS decoded_q,
    (
        (registry.linear_anchor_id - (registry.linear_anchor_id % grid.linear_radix))
        / grid.linear_radix
    ) - grid.coordinate_offset AS decoded_r,
    -(
        (registry.linear_anchor_id % grid.linear_radix) - grid.coordinate_offset
    ) - (
        (
            (registry.linear_anchor_id - (registry.linear_anchor_id % grid.linear_radix)
        ) / grid.linear_radix
        ) - grid.coordinate_offset
    ) AS decoded_s,
    CASE
        WHEN registry.q = (registry.linear_anchor_id % grid.linear_radix) - grid.coordinate_offset
         AND registry.r = (
             (registry.linear_anchor_id - (registry.linear_anchor_id % grid.linear_radix))
             / grid.linear_radix
         ) - grid.coordinate_offset
         AND registry.s = -registry.q - registry.r
        THEN 'BIJECTION_AUDIT_PASS'
        ELSE 'BIJECTION_AUDIT_FAIL'
    END AS inverse_audit_status,
    registry.transform_method
FROM phoenix_hex_anchor_registry AS registry
JOIN phoenix_hex_grid_definitions AS grid
    ON grid.hex_grid_definition_id = registry.hex_grid_definition_id;
