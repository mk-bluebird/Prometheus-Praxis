# PHX-HEX-CORRIDOR-20260817

Reversible local hex-anchor binning and resolution-selection tools for Phoenix
heat-island and canal-corridor analysis.

Files:
- cpp/phoenix_hex_transform.cpp
- lua/phoenix_hex_transform.lua
- kotlin/HexBinResolutionSelector.kt
- sql/phoenix_hex_anchor_corridor.sql

Coordinate policy:
- Input latitude and longitude remain stored with the generated anchor.
- A declared local origin latitude/longitude and meters-per-degree approximation
  define a reversible local planar frame:
    x_m = (lon - lon0) * metersPerDegreeLongitude
    y_m = (lat - lat0) * metersPerDegreeLatitude
    lon = lon0 + x_m / metersPerDegreeLongitude
    lat = lat0 + y_m / metersPerDegreeLatitude
- Hex conversion uses pointy-top axial coordinates with side length a:
    qf = (sqrt(3)/3 * x_m - 1/3 * y_m) / a
    rf = (2/3 * y_m) / a
- Cube rounding creates integer anchor coordinates:
    q + r + s = 0, with s = -q-r.
- Anchor identifiers include geometry version, origin, side length, q, r, and s.
- The inverse returns the center of the assigned bin, not the original point.
  The original latitude/longitude columns preserve full reversibility.

Use an approved projected coordinate reference system for deployment-grade meter
distances. The local conversion supplied here is explicit and auditable but is a
small-area approximation, not a replacement for GIS transformation.

Resolution selection:
- Candidate bin sizes are evaluated using leave-one-out prediction error from
  each bin mean, plus a sparse-bin penalty.
- The chosen size minimizes:
    objective = RMSE + sparsePenaltyWeight * sparseFraction
- Minimum samples per bin prevents reporting a resolution that appears detailed
  but lacks data support.
- This is a data-driven smoothing heuristic; it does not prove recovery of every
  physical spatial gradient.
