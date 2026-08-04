# PFAS Lyapunov Residual Recurrence and H3↔Canal Crosswalk

## PFAS Lyapunov Residual in SQL

- The SQL shard `sql/pfas_lyapunov_residual.sql` defines:
  - `pfas_corridor_telemetry` for PFAS concentration time-series per canal segment.
  - Helper view `pfas_corridor_deltas` computing:
    - `δ_n = pfas_n - pfas_{n-1}` and `dt_n = t_n - t_{n-1}` via window functions.
  - View `pfas_lyap_residual` that approximates the Lyapunov residual recurrence:

    \[
    \text{lyap\_res}(t_{n}) \approx
    \sum_{k=1}^{n-1} dt_k \frac{\ln\left|\delta_k / \delta_{k-1}\right|}{t_k - t_0},
    \]

    using cumulative SUM over each `canal_segment`. The simplified feed view `pfas_lyap_corridor_feed` exposes `(canal_segment, timestamp_s, pfas_ug_l, lyap_res)` for consumption by a Lua corridor analyser.

- This recurrence provides an online approximation of the maximal Lyapunov exponent trajectory from telemetry, enabling corridor stability assessment without constructing a digital twin.

## H3 Hex ↔ Phoenix Canal Segment Crosswalk (Lua + SQLite R*Tree)

- The Lua shard `lua/h3_to_canal_crosswalk.lua` implements a data-integration object:
  - Assumes:
    - `hex_cell_catalog` table with H3 indices and hex center coordinates.
    - `canal_segments_geo` with canal segment centers and `canal_capacity_m3_s`.
    - `canal_segments_rtree` R*Tree index over canal bounding boxes.
  - Given an `h3_index`, the script:
    1. Queries `hex_cell_catalog` for hex center `(lat, lon)`.
    2. Uses the R*Tree to select canal segments whose bounding boxes contain the hex center.
    3. Computes Euclidean distance squared between hex and candidate canal centers, selecting the nearest.
    4. Returns a Lua table with `canal_segment` and `canal_capacity_m3_s`.

- This crosswalk enables the FOG router to route runoff from hex-cells with explicit awareness of the capacity of the nearest canal segment, integrating spatial hex-anchor logic with hydraulic constraints for eco-safe cyboquatic operations.

Technical justification: The Lyapunov residual SQL recurrence converts PFAS telemetry into a tractable stability signal that can be streamed into Lua-based corridor analysers, while the H3↔canal crosswalk bridges hex-based thermal/land-surface models to physically constrained canal segments via spatial indexing, ensuring that FOG routing decisions respect both environmental and hydraulic capacities.
