# Cyboquatic System Workload MCP Tools

This document defines the Model Context Protocol (MCP) tools for interacting with the cyboquatic system-level workload optimization and telemetry core. All tools are read-only and operate on the SQLite schema defined in sql/system_workload_core_schema.sql.

## 1. Tool: tool_system_kwh_per_m3

Purpose:

- Retrieve energy per unit treated (kWh/m³) per asset and window for analysis and reporting.

Input parameters:

- window_label (TEXT): time window label (e.g., "2026Q3", "2026-08-01").

Output schema:

- window_label (TEXT)
- asset_type (TEXT)
- asset_id (INTEGER)
- total_energy_kWh (REAL)
- total_volume_m3 (REAL)
- kWh_per_m3 (REAL)

Query backing:

- SELECT * FROM v_system_kwh_per_m3 WHERE window_label = :window_label;

## 2. Tool: tool_system_carbon_summary

Purpose:

- Summarize net carbon emissions per asset and window.

Input parameters:

- window_label (TEXT)

Output schema:

- window_label (TEXT)
- asset_type (TEXT)
- asset_id (INTEGER)
- total_carbon_kgCO2 (REAL)

Query backing:

- SELECT * FROM v_system_carbon_balance WHERE window_label = :window_label;

## 3. Tool: tool_system_risk_status

Purpose:

- Report maximum hydraulic, PFAS, and biodiversity risk coordinates per node and window.

Input parameters:

- window_label (TEXT)

Output schema:

- window_label (TEXT)
- node_id (INTEGER)
- r_hyd_max (REAL)
- r_pfas_max (REAL)
- r_bio_max (REAL)

Query backing:

- SELECT * FROM v_system_risk_corridor WHERE window_label = :window_label;

## 4. Tool: tool_system_workload_plan

Purpose:

- Retrieve advisory workload plan decisions per window for review and scheduling.

Input parameters:

- window_label (TEXT)

Output schema:

- plan_id (INTEGER)
- window_label (TEXT)
- node_id (INTEGER)
- asset_id (INTEGER)
- assigned_flow_m3h (REAL)
- energy_kWh (REAL)
- carbon_kgCO2 (REAL)

Query backing:

- SELECT * FROM system_workload_plan WHERE window_label = :window_label;

## 5. Usage Notes

- All tools are READONLY and perform SELECT queries only.
- AI assistants should use these tools to answer natural language questions regarding:
  - Energy per unit treated per asset/window.
  - Net carbon contributions per asset/window.
  - Risk corridor compliance per window and node.
  - Proposed workload distribution plans and their energy/carbon profiles.

These tools pre-wire the system for AI-assisted analysis and reporting, while keeping actuation and control logic in separate, governed stacks.
