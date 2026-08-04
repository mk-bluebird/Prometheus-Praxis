-- File: sql/hex_cate_storage.sql
PRAGMA foreign_keys = ON;

-- Schema for storing heterogeneous treatment effects (CATE) of cyboquatic interventions
-- on afternoon LST per hex-cell.

-- Hex registry (assumed existing); referenced here for completeness.
CREATE TABLE IF NOT EXISTS hex_cell_catalog (
    h3_index TEXT PRIMARY KEY,
    basin_id TEXT NOT NULL,
    center_lat REAL NOT NULL,
    center_lon REAL NOT NULL
);

-- Historical intervention records per hex-cell.
CREATE TABLE IF NOT EXISTS hex_interventions (
    intervention_id INTEGER PRIMARY KEY AUTOINCREMENT,
    h3_index TEXT NOT NULL,
    intervention_type TEXT NOT NULL,   -- e.g., 'canopy', 'cool_pavement', 'aeration_basin'
    intervention_date_utc TEXT NOT NULL,
    cost_unit REAL NOT NULL,           -- cost per unit intervention (e.g., per m^2 canopy)
    FOREIGN KEY(h3_index) REFERENCES hex_cell_catalog(h3_index)
);

CREATE INDEX IF NOT EXISTS idx_hex_interventions_h3_date
    ON hex_interventions(h3_index, intervention_date_utc);

-- CATE storage: conditional average treatment effect per hex-cell and intervention type.
-- This is produced by causal inference methods (double ML, causal forests) offline
-- and stored for online querying by the Kotlin dashboard.
CREATE TABLE IF NOT EXISTS hex_cate (
    cate_id INTEGER PRIMARY KEY AUTOINCREMENT,
    h3_index TEXT NOT NULL,
    intervention_type TEXT NOT NULL,
    cate_lst_drop_k REAL NOT NULL,     -- predicted afternoon LST reduction (K) per unit intervention
    cate_confidence REAL NOT NULL,     -- confidence or standard error (0..1)
    cost_unit REAL NOT NULL,           -- cost per unit intervention (same units as hex_interventions.cost_unit)
    FOREIGN KEY(h3_index) REFERENCES hex_cell_catalog(h3_index)
);

CREATE INDEX IF NOT EXISTS idx_hex_cate_h3_type
    ON hex_cate(h3_index, intervention_type);

-- View: compute LST reduction per unit cost and allow top-k selection.
CREATE VIEW IF NOT EXISTS hex_cate_canopy_value AS
SELECT
    c.h3_index,
    c.intervention_type,
    c.cate_lst_drop_k,
    c.cost_unit,
    c.cate_confidence,
    (c.cate_lst_drop_k / c.cost_unit) AS lst_drop_per_cost
FROM hex_cate c
WHERE c.intervention_type = 'canopy';

-- Example query: return top-k hexes where additional canopy cover yields
-- the largest predicted LST reduction per unit cost, subject to a minimum
-- confidence threshold.

-- Parameterised query (Kotlin will bind :k and :confidence_min):
-- SELECT h3_index, cate_lst_drop_k, cost_unit, lst_drop_per_cost
-- FROM hex_cate_canopy_value
-- WHERE cate_confidence >= :confidence_min
-- ORDER BY lst_drop_per_cost DESC
-- LIMIT :k;
