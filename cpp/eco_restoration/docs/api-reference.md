<!-- File: docs/api-reference.md -->
# API Reference

## C++ core

| Area | Primary modules | Output |
|---|---|---|
| Admission | `eco_workload_admission`, `quality_weighted_lane_evaluator`, `uncertainty_aware_lane_decision` | Lane action, K/E/R, confidence intervals |
| Water and canals | Seepage filters, water-quality CUSUM, MPC, inundation routing | Flow, water risk, review events |
| Spatial analysis | Hex joins, thermal aggregation, ET, GDE, renewable map | Per-hex environmental layers |
| Restoration | Soil carbon, action ILP, multi-objective planner, scenarios | Prioritized, constrained plans |
| Forecasting | Carbon GARCH, ecological ARIMA, Sobol analysis | Forecasts and uncertainty |
| Interfaces | Dashboard, citizen ingestion, LoRaWAN bridge, correction server | Local JSON endpoints |

## Lua modules

Lua modules remain adapters for validated CSV, SQLite-backed review, dependency lookup, queue routing, and conflict presentation. They should consume only normalized, quality-tagged values.

## SQL modules

SQL schemas define strict evidence tables, time aggregates, version vectors, Pareto persistence, risk alerts, and indexes. Execute schema files against SQLite before running producers or dashboards.
