# Trend-Based FOG Routing and Hyperbolic KER Discounting

## Lua FOG Router with Recent History

- `lua/fog_router_trend.lua` extends the FOG router to use recent telemetry history:
  - Fetches the last 6 samples for a given `(node_id, canal_id)` via a piped `sqlite3` command:
    - `SELECT timestamp_s, dissolved_o2_mg_l, pfas_ug_l ... ORDER BY timestamp_s DESC LIMIT 6;`
  - Performs linear regression in Lua on:
    - Dissolved O₂ (`dissolved_o2_mg_l`) and
    - PFAS concentration (`pfas_ug_l`).
  - Computes slopes `b_do` and `b_pfas`:
    - If `b_do > 0` and `b_pfas < 0`, the trend is improving (`FOG:TREND_IMPROVING`).
    - Otherwise, the trend is degrading (`FOG:TREND_DEGRADING`).
- This wiring pattern allows FOG routing decisions to incorporate temporal trends, rather than single snapshots, improving eco-restoration safety and responsiveness.

## Hyperbolic Discounting of KER Eco-Impact

- `aln_v2/ker_hyperbolic_discount.aln` defines:
  - A hyperbolic discount factor for past emissions:
    - `d(n) = 1 / (1 + a * n)` with `a > 0`, giving higher weight to recent net carbon flux `e_n` and gradually discounting older emissions.
  - Discounted eco-impact:
    - `ker_e_discounted_n = Σ d(i) * e_i`, implemented recursively:
      - `ker_e_discounted_n = ker_e_discounted_{n-1} + d(n) * e_n`.
- ALN invariants:
  - `KerEDiscountedRecurrence` ensures the discounted sum is computed correctly across samples.
  - `KerEDiscountedCarbonNegative` enforces `ker_e_discounted <= 0` for all sample indices, guaranteeing temporally consistent carbon-negative behavior under hyperbolic discounting.
- SQL recursive CTE wiring:
  - A `WITH RECURSIVE` CTE computes `ker_e_discounted` over `TelemetrySample` rows using the same recurrence and discount factor.
  - A constraint or periodic check that no `ker_e_discounted > 0` rows exist enforces the ALN invariant at the DB level.

Technical justification: The Lua trend-based FOG router uses short-term dissolved O₂ and PFAS trends to classify corridor behavior, providing more robust routing decisions than single-sample analysis. Hyperbolic discounting of KER eco-impact implements a mathematically grounded temporal consistency mechanism, where recent emissions matter more but the overall discounted sum must remain non-positive, ensuring that long-horizon eco-restoration policies remain carbon-negative even under stochastic fluxes, with ALN invariants and SQL CTEs providing formal and operational enforcement.
