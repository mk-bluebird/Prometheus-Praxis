<!-- filename: eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/docs/10axis_to_risk_mapping.md
     purpose : Exhaustive table mapping each of the ten measurement axes to its
               risk coordinate, normalization function, weight, and plane. -->

# 10‑Axis Telemetry → Lyapunov Risk Coordinates

This document is the normative reference for translating raw AI‑data‑centre telemetry into the
risk coordinates used by `AiDatacenterNode2026v1.aln`.  All corridors are initial values;
they may only be **tightened** over time (always‑improve rule).

## Primary Constraint Axes (feed into Vt)

| Axis / Metric               | Ideal Value | Corridor Ceiling | Normalisation Rule               | Plane              | Weight |
|-----------------------------|-------------|------------------|----------------------------------|--------------------|--------|
| **CUE** (kg CO₂/kWh)        | 0.0         | 0.5              | `r = (CUE - 0.0) / (0.5 - 0.0)` | AI_CARBON          | 0.25   |
| **Eco‑per‑Joule** (benefit/J) | 1.0       | 0.2              | `r = (1.0 - x) / (1.0 - 0.2)` if x < 1.0, else 0 | AI_CARBON | 0.25 |
| **PUE** (ratio)             | 1.09        | 1.5              | `r = (PUE - 1.09) / (1.5 - 1.09)` | AI_POWER         | 0.15   |
| **Eco‑Task Ratio** (fraction)| 1.0        | 0.4              | `r = (1.0 - x) / (1.0 - 0.4)`    | AI_ECO_RATIO      | 0.15   |
| **WUE** (L/kWh)             | 0.5         | 2.0              | `r = (WUE - 0.5) / (2.0 - 0.5)`  | AI_WATER_MATERIALS| 0.10   |
| **Materials Intensity** (kgCO₂/TFLOP‑yr)| 0.1 | 1.0        | `r = (x - 0.1) / (1.0 - 0.1)`    | AI_WATER_MATERIALS| 0.05   |
| **Topology Risk** (score)   | 0.0         | 0.3              | `r = x / 0.3`                    | AI_TOPOLOGY       | 0.05   |

All `r` values are clamped to `[0,1]`.  If a metric exceeds its ceiling, `r = 1`.

## Secondary Guidance Axes (affect K‑boost only, not Vt)

| Axis / Metric               | Ideal Value | How It Boosts K                      |
|-----------------------------|-------------|--------------------------------------|
| **Tokens per Joule**        | 1e6         | `boost = 0.4 * clamp(x / 1e6, 0, 1)` |
| **Utilisation** (fraction)  | 0.9         | `boost = 0.3 * clamp(x / 0.9, 0, 1)` |
| **ERE** (heat reuse ratio)  | 1.0         | `boost = 0.3 * clamp(x / 1.0, 0, 1)` |

Total K‑boost = `0.05 * ( boost_tokens + boost_util + boost_ere )`.
