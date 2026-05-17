-- filename ecosafety_grammar_core.sql
-- destination Eco-Fort/db/db_ecosafety_grammar_core.sql

PRAGMA foreign_keys = ON;

-- Base table (already defined in your spine, shown for context)
-- CREATE TABLE plane_weights (...);

-- Revoke broad access first
REVOKE ALL ON plane_weights FROM PUBLIC;
REVOKE ALL ON plane_weights FROM ecosafety_research;
REVOKE ALL ON plane_weights FROM ecosafety_expprod;

-- Grant full read to PROD lane only
GRANT SELECT ON plane_weights TO ecosafety_prod;

DROP VIEW IF EXISTS v_plane_weights_research;

CREATE VIEW v_plane_weights_research AS
SELECT
  plane_code,
  coord_code,
  -- Masked weight: expose only qualitative band, not actual numeric weight
  weight_band_label,          -- e.g. 'LOW', 'MEDIUM', 'HIGH'
  nonoffsettable_flag,
  versiontag,
  status
FROM plane_weights;

-- Grant RESEARCH access to masked view only
GRANT SELECT ON v_plane_weights_research TO ecosafety_research;

-- Optional: EXPPROD can see masked plus some numeric fields
DROP VIEW IF EXISTS v_plane_weights_expprod;

CREATE VIEW v_plane_weights_expprod AS
SELECT
  plane_code,
  coord_code,
  weight_band_label,
  nonoffsettable_flag,
  band_min,   -- limited numeric band info
  band_max,
  versiontag,
  status
FROM plane_weights;

GRANT SELECT ON v_plane_weights_expprod TO ecosafety_expprod;
