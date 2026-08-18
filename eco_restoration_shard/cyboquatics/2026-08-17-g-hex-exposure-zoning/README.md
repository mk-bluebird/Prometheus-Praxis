# PHX-HEX-EXPOSURE-ZONING-20260817

Non-actuating domain (g) artifact set for deterministic canal-node zoning against
a conservative surcharge-breach radius and indexed SQLite blast-radius queries.

Files:
- lua/hex_exposure_zone.lua
- kotlin/HexExposureZone.kt
- sql/hex_anchor_blast_radius_indices.sql
- aln/hex_exposure_zoning_20260817.aln2

Zone rule:
  SAFE     if d > R_cons
  CAUTION  if R_cons / 2 < d <= R_cons
  EXCLUDE  if 0 <= d <= R_cons / 2

where:
- d is the supplied node-to-breach distance in meters.
- R_cons is a previously validated conservative radius in meters.

Hex anchors are stable, human-auditable labels. They are not coordinates, spatial
proof, or a substitute for GIS/geodesic distance calculation. Provide a distance
computed through an approved local-survey or GIS workflow; the Lua/Kotlin tools
only classify it deterministically.

SQLite indices:
- The assessment index begins with zone and conservative_radius_m as requested.
- It includes event/node fields to cover common zone/radius retrievals.
- The event index supports recent events per anchor/node.
- Query plans depend on SQLite version, table cardinality, predicates, statistics,
  and selected columns. Use EXPLAIN QUERY PLAN on actual deployment data before
  claiming a particular query avoids a full table scan.
