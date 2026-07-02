# Workload Window Index

This document provides an overview of all workload window artifacts in the repository for humans and AI-chat platforms.

## ALN Particle Definition

- **File:** `core/aln/ecosafety-nanoswarm-urban-core/src/workload_node_window.aln`
- **Purpose:** Defines the `ecosafety.workload.node.window.v1` particle schema.
- **Key Fields:**
  - `base`: Shard row identity (shardid, timestamputc, objectid, KER).
  - `nodeid`, `assetid`: Node and optional asset identifiers.
  - `window_start_utc`, `window_end_utc`: Time window bounds.
  - `energy_req_j`, `energy_surplus_j`: Energy totals in Joules.
  - `accepted_fraction`, `rejected_fraction`, `rerouted_fraction`: Workload fractions (0..1).
  - `mean_vt_before`, `mean_vt_after`, `mean_delta_vt`: Lyapunov residual traces.
  - `corridor_status`, `decision_mode`: Corridor semantics and decision mode.
  - `window_ker_id`: Window-level KER triplet reference.

## SQL Schema

- **File:** `db/db_ecosafety_workload_window.sql`
- **Table:** `ecosafety_workload_node_window`
- **Purpose:** Persistent storage for workload node windows, aligned with the ALN particle and Rust struct.

## Rust Types

- **File:** `crates/ecosafety-nanoswarm-urban-core/src/workload_window.rs`
  - **Struct:** `WorkloadNodeWindow` – mirrors the ALN particle and SQL table.
- **File:** `crates/ecosafety-nanoswarm-urban-core/src/workload_window_query.rs`
  - **Function:** `list_workload_node_windows()` – pure query function with optional filters.
- **File:** `crates/ecosafety-nanoswarm-urban-core/src/workload_window_summary.rs`
  - **Struct:** `WorkloadWindowSummary` – aggregated statistics for a node/time window.
  - **Function:** `summarize_workload_node_window()` – computes means and violation counts.

## Function Catalog

- **File:** `core/aln/prometheus-praxis/functioncatalog/workloadwindow_functions.aln`
- **Functions:**
  - `ecosafety.workload.node.window.list.v1` – lists workload windows (Rust/Lua).
  - `ecosafety.workload.node.window.summary.v1` – summarizes workload windows (Rust/Lua).
- **Properties:** Non-actuating, diagnostic-only, high-K/high-E/low-R.

## Lua Client Module

- **File:** `runtime/lua/prometheus_praxis/workloadwindow_client.lua`
- **Functions:**
  - `M.get_workload_node_windows(input)` – stub for listing windows.
  - `M.summarize_workload_node_window(input)` – stub for summarizing windows.
- **Usage:** AI-chat tools can inspect this module to understand entrypoints and expected input/output shapes.
