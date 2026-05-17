-- 45. ColumnInvariants shard for altitude bands and time-varying weights
-- filename: Eco-Fort/db/dbcolumn_invariants.sql
-- destination: Eco-Fort/db/dbcolumn_invariants.sql

PRAGMA foreign_keys = ON;

-- ColumnInvariants captures per-geohex, altitude band, and time-varying
-- weights for planes/coordinates, used to adjust Vt and KER locally.[file:7][file:11]

CREATE TABLE IF NOT EXISTS geohex_column (
  columnid        INTEGER PRIMARY KEY AUTOINCREMENT,
  geohex_id       TEXT NOT NULL,    -- e.g. 'PHX-H3-8fc3999'
  region          TEXT NOT NULL,    -- e.g. 'Phoenix-AZ'
  description     TEXT
);

-- Overlapping altitude bands allowed; precedence is resolved by
-- effective_time range and band specificity in queries.[file:7]

CREATE TABLE IF NOT EXISTS column_invariant (
  invariantid     INTEGER PRIMARY KEY AUTOINCREMENT,
  columnid        INTEGER NOT NULL REFERENCES geohex_column(columnid)
                    ON DELETE CASCADE,
  planeid         TEXT NOT NULL,    -- e.g. 'energy','biodiversity','materials'
  coord_key       TEXT NOT NULL,    -- e.g. 'renergy', 'rbiodiversity'
  alt_min_m       REAL NOT NULL,    -- inclusive lower bound
  alt_max_m       REAL NOT NULL,    -- exclusive upper bound
  tstartutc       TEXT NOT NULL,    -- ISO-8601: invariant active from
  tendutc         TEXT NOT NULL,    -- ISO-8601: invariant active until
  weight_factor   REAL NOT NULL,    -- multiplicative factor on plane weight w_j
  vt_penalty      REAL NOT NULL,    -- additive penalty on residual term
  notes           TEXT
);

CREATE INDEX IF NOT EXISTS idx_column_invariant_lookup
  ON column_invariant (columnid, planeid, coord_key, alt_min_m, alt_max_m, tstartutc, tendutc);

-- Example: 3D hex column in dense urban canopy
--
-- geohex_id: 'PHX-H3-URBAN-CANOPY-01'
--
--   Band 1: ground layer (0–20 m), high materials + low biodiversity
--           weight due to impermeable surfaces and sparse canopy.
--
--   Band 2: mid canopy (20–40 m), high biodiversity weight (tree crowns,
--           bird corridors), moderate materials weight.
--
--   Band 3: rooftop / super-canopy (40–80 m), high energy weight
--           (solar, HVAC exhaust), moderate biodiversity weight.[file:7]

INSERT INTO geohex_column (geohex_id, region, description)
VALUES (
  'PHX-H3-URBAN-CANOPY-01',
  'Phoenix-AZ',
  'Dense urban canopy hex column with multi-layer invariants'
);

-- Retrieve assigned columnid for example (assuming single row):
-- SELECT columnid FROM geohex_column WHERE geohex_id='PHX-H3-URBAN-CANOPY-01';

-- Example invariants (pseudocode inserts; columnid to be resolved at migration time):
-- Ground layer: 0–20 m, materials up-weight, biodiversity down-weight
-- INSERT INTO column_invariant(columnid, planeid, coord_key, alt_min_m, alt_max_m,
--   tstartutc, tendutc, weight_factor, vt_penalty, notes)
-- VALUES
--   ( <columnid>, 'materials', 'rmaterials', 0.0, 20.0,
--     '2026-01-01T00:00:00Z', '2027-01-01T00:00:00Z',
--     1.5, 0.02,
--     'Impervious ground layer, higher material risk weight' ),
--   ( <columnid>, 'biodiversity', 'rbiodiversity', 0.0, 20.0,
--     '2026-01-01T00:00:00Z', '2027-01-01T00:00:00Z',
--     0.7, 0.00,
--     'Low biodiversity weight at street level' );
--
-- Mid canopy: 20–40 m, biodiversity heavily weighted, dynamic over time
--   (e.g., increased weight during nesting season).
--
-- INSERT INTO column_invariant(...)
--   ( <columnid>, 'biodiversity', 'rbiodiversity', 20.0, 40.0,
--     '2026-03-01T00:00:00Z', '2026-08-31T23:59:59Z',
--     1.8, 0.05,
--     'High biodiversity weight during bird nesting season' );
--
-- INSERT INTO column_invariant(...)
--   ( <columnid>, 'biodiversity', 'rbiodiversity', 20.0, 40.0,
--     '2026-09-01T00:00:00Z', '2027-02-28T23:59:59Z',
--     1.2, 0.02,
--     'Moderate biodiversity weight outside nesting season' );
--
-- Rooftop: 40–80 m, energy up-weighted due to HVAC / solar hardware
--
-- INSERT INTO column_invariant(...)
--   ( <columnid>, 'energy', 'renergy', 40.0, 80.0,
--     '2026-01-01T00:00:00Z', '2027-01-01T00:00:00Z',
--     1.6, 0.03,
--     'High energy plane weight for rooftop infrastructure' );
--
-- Overlapping bands:
--   Overlaps are resolved by:
--     - selecting all rows where:
--         alt_min_m <= z < alt_max_m
--         AND tstartutc <= t < tendutc
--       and applying combined factors (e.g., multiplicative weight_factor,
--       additive vt_penalty) in the residual kernel.[file:7][file:11]
