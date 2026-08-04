# Distributed Hex-Anchor Convergence and Surcharge Breach Index Strategy

## Hex-Anchor Optimisation Convergence

The C++ file `cpp/simulation/hex_anchor_convergence_note.cpp` encodes the convergence proof for distributed hex-anchor optimisation:

- Global objective:
  - `J(g) = Σ f_i(g_i) + (λ/2) Σ w_ij (g_i - g_j)^2`, where:
    - `f_i(g_i)` is the local LST/energy-budget cost per hex-cell.
    - `(λ/2) Σ w_ij (g_i - g_j)^2` is a graph-Laplacian penalty enforcing smoothness.
- Convexity conditions:
  - Each `f_i` is convex in `g_i`, e.g., quadratic in green-fraction with diminishing returns.
  - The Laplacian term is convex due to positive semi-definite graph Laplacian.
- Under synchronised step sizes `α` satisfying `0 < α < 2 / (L_f + λ λ_max(L))`,
  gradient descent on `J` converges to the unique global optimum `g*`, because
  the hex-wise updates collectively implement a contraction mapping on a strictly
  convex objective. The energy-budget model’s quadratic LST anomaly ensures the
  required convexity.

## Surcharge Breach SQLite Covering Index

The SQL shard `sql/surcharge_breach_index_strategy.sql` defines:

- `blast_radius_surcharge` table for surcharge events, with key fields:
  - `canal_segment`, `upstream_level_m`, `soil_moisture_class`,
    `forecast_surge_2h_m`, `breach_probability`, `flood_polygon_id`.
- A covering index `idx_surcharge_cover` on:
  - `(canal_segment, upstream_level_m, soil_moisture_class, breach_probability,
      forecast_surge_2h_m, flood_polygon_id)`.

This index supports queries like:

```sql
SELECT canal_segment, upstream_level_m, soil_moisture_class,
       breach_probability, flood_polygon_id
FROM blast_radius_surcharge
WHERE forecast_surge_2h_m >= :surge_threshold
  AND breach_probability > 0.8;
```

allowing SQLite’s EXPLAIN QUERY PLAN to show an index-only scan, since all
required columns are covered by the index. This strategy provides fast
retrieval of high-risk segments under 2-hour surge forecasts, crucial for
real-time eco-safe blast-radius prediction in cyboquatic canal systems.

Technical justification: The convergence object formalises hex-anchor optimisation as gradient descent on a convex global objective combining local energy-budget costs and Laplacian smoothness, guaranteeing unique global optimum under standard step-size constraints. The surcharge breach index design uses a composite covering index to enable index-only scans for high-probability breach queries, improving latency and energy efficiency of real-time hazard prediction workflows.
