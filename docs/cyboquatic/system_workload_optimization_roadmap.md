# From Simulation to System: Cyboquatic Workload Optimization Roadmap

## 1. Overview

This document describes the system-level roadmap for moving cyboquatic workload optimization from simulation artifacts into a production-ready, telemetry-backed, carbon-negative analysis and planning layer within Prometheus-Praxis. It focuses on pumps, aerators, and mixers operating in treatment basins and canals, governed by KER (Knowledge, Eco-Impact, Residual Risk) triads and Lyapunov residuals.

The target architecture:

- Uses real telemetry from SCADA/PLC systems without direct actuation.
- Normalizes data into a self-contained SQLite core.
- Computes workload metrics (energyreqJ, ΔVt, KER) per cycle and per system window.
- Exposes read-only analysis tools via MCP for AI-assisted querying and reporting.
- Binds optimization and reporting to ALN v2 governance particles for DID-scoped accountability.

## 2. Current Cyboquatic Artifacts

### 2.1 Core Computational Engine

Artifact: cpp/eco_restoration/cyboquatic_workload_model.cpp

Responsibilities:

- Define WorkloadCycle, including:
  - cycle_id
  - hex_id
  - canal_node_id
  - energyreqJ (Joules per cycle)
  - risk planes before/after (hydraulics, energy, topology, biodiversity)
  - ΔVt (Lyapunov residual change)
  - K, E, R scores in [0, 1].

- Compute Lyapunov residual:
  V_t = Σ w_j r_{t,j}²

- Derive KER triad:
  - K = fraction of planes not worsened between before and after.
  - E = eco-impact proxy combining low energyreqJ and low average risk.
  - R = max residual risk coordinate after the cycle.

- Provide CLI to insert cycles into SQLite workload_cycle table.

### 2.2 Governance Schema and KER Corridors

Artifact: sql/cyboquatic_workload_schema.sql

Tables:

- canal_node: binds canal_node_id, hex_id, DID owner, and KER corridor parameters (ker_K_min, ker_E_min, ker_R_max).
- workload_cycle: stores per-cycle telemetry and KER metrics with constraints:
  - energyreqJ ≥ 0
  - risk planes in [0, 1]
  - delta_Vt bounded
  - K, E, R in [0, 1].

- ker_window: aggregates KER triads over time windows.
- fog_flow: stores FOG and unmodeled media metrics.

Enforcement:

- CHECK constraints and AFTER INSERT trigger enforcing:
  - K ≥ canal_node.ker_K_min
  - E ≥ canal_node.ker_E_min
  - R ≤ canal_node.ker_R_max

Ownership:

- canal_node.owner_did must equal the DID anchor.
- This binds all canal telemetry and KER corridors to a single governance identity.

### 2.3 Interoperability Artifacts

Artifacts:

- java/cyboquatic/TelemetryClient.java
  - Insert fog_flow rows.
  - Query workload_cycle for high-risk or energy-inefficient cycles.

- kotlin/cyboquatic/FogRouter.kt
  - Join fog_flow and workload_cycle.
  - Evaluate route decisions:
    - SAFE_TREATMENT_BASIN
    - EFFICIENCY_RETUNING
    - STANDARD_CANAL_FLOW

- lua/cyboquatic/fog_router.lua
  - Embedded FOG routing and advisory logic for resource-constrained controllers.

These provide a path to integrate telemetry consumers and advisory logic in multiple environments while leaving actuation to separate stacks.

### 2.4 ALN Governance Particle

Artifact: aln/cyboquatic_workload_ker.aln

Responsibilities:

- Bind workload governance to ID did_owner.
- Define risk planes:
  - hydraulics, energy, topology, biodiversity
  - gold/amber/red corridors
  - non-offsettable flags for critical planes.

- Define KER triad invariants:
  - Minimum K, E per window.
  - Maximum R per window.

- Map SQL binding:
  - database eco_restoration_shard.db
  - table workload_cycle and ker_window.

This ensures a cryptographically-scoped, declarative definition of ecologically safe corridors.

## 3. System-Level Research Gaps

Identified gaps:

1. Telemetry integration:
   - Hardcoded example values need replacement with real SCADA/PLC telemetry.
   - No current ingestion of pump speeds, blower Hz, motor kW, DO, BOD/COD, PFAS.

2. Risk and eco-impact calibration:
   - Risk coordinates are abstract, not empirically mapped to measured outcomes.
   - Eco-impact E needs explicit link to kWh/m³, effluent quality, and contamination reduction.

3. Probabilistic hydraulic risk:
   - No use of surcharge or overflow probabilities.
   - Deterministic thresholds ignore storm variability and rare events.

4. Dashboard views and AI interfaces:
   - No summary views for KER compliance, energy per unit treated, or carbon footprints.
   - MCP tools are not yet defined for KER and energy/carbon querying.

5. System-level optimization:
   - Local cycles are tracked; global distribution across basins/assets is not yet optimized.
   - No formal problem statement or solver for minimizing energy and carbon under KER constraints.

## 4. Mathematical Formulations

### 4.1 Eco-Impact Function

Define eco-impact E per cycle:

E = w_energy * (1 - energyreqJ / energyreqJ_max)
  + w_BOD * BOD_removal_rate
  + w_PFAS * PFAS_removal_rate
  - w_risk * R

Where:

- energyreqJ: measured energy per cycle.
- energyreqJ_max: upper bound (normalization).
- BOD_removal_rate: fraction of BOD removed (0–1).
- PFAS_removal_rate: fraction of PFAS removed (0–1).
- R: residual risk coordinate.
- w_*: weights tuned via multi-objective calibration.

This makes E a monotone, interpretable score for mixed energy and eco performance.

### 4.2 Stability via Lyapunov and Safe Step

Lyapunov residual:

V_t = Σ_j w_j r_{t,j}²

Safe step score:

s_t = K_t * E_t - R_t

Stability constraint:

V_{t+1} - V_t ≤ -α * s_t,  α > 0

Interpretation:

- If s_t > 0 (good cycle), enforce V_{t+1} < V_t via constraints.
- If s_t ≤ 0, flag cycles as unsafe, reject scheduling decisions.

Implement by:

- Evaluating s_t for each proposed cycle.
- Rejecting cycles if the inequality would be violated.
- Recording s_t and ΔVt for audit.

### 4.3 Probabilistic Hydraulic Risk Coordinate

Define hydraulic risk:

r_hyd = γ_1 * P_surcharge + γ_2 * P_overflow

Where:

- P_surcharge: probability of hydraulic surcharge at node impacting a hex.
- P_overflow: probability of structural overflow or uncontrolled discharge.
- γ_*: scaling coefficients.

Probabilities can be derived from:

- PDE-based surcharge energy fields.
- Historical rainfall and storm return periods.
- Manhole/overflow models.

Use r_hyd as:

- One component of V_t and KER triads.
- Basis for threshold-based advisory messages and alerts.

### 4.4 System-Level Workload Distribution

Variables:

- i ∈ N: system nodes (basins, canal segments, treatment stages).
- a ∈ A: assets (pumps, blowers, mixers).
- t: discrete time index.
- x_{i,a,t}: workload assignment (flow or load fraction handled by asset a at node i at time t).

Objective:

Minimize overall energy and carbon:

min_x Σ_t [ Σ_{i,a} E_a(x_{i,a,t}) + λ_carbon Σ_{i,a} C_a(x_{i,a,t}) ]

Subject to:

- Flow conservation:
  Σ_a x_{i,a,t} = Q_{i,t}  for all i,t

- Asset capacity:
  x_{i,a,t} ≤ Q_{a,max}  for all i,a,t

- Risk corridors:
  r_{i,hyd}(t) ≤ r_hyd_max
  r_{i,PFAS}(t) ≤ r_PFAS_max
  r_{i,bio}(t) ≤ r_bio_max

- Window-level KER invariants:
  K_W_sys ≥ K_min
  E_W_sys ≥ E_min
  R_W_sys ≤ R_max

Metrics:

- Energy per unit treated (kWh/m³):
  kWh_per_m3 = (Σ_t Σ_{i,a} E_a(x_{i,a,t})) / Σ_t V_treated,t

- Net carbon:
  CO2e_net = Σ_t Σ_{i,a} C_a(x_{i,a,t}) - Σ_t EnergyRecovery(t) - Σ_t ResourceRecovery(t)

This is the core optimization problem the system-level C++ optimizer must solve.

## 5. Next-Step Research Objectives

### Objective 1: Unified Machinery Telemetry Schema

Actions:

- Add tables:
  - machinery_asset(asset_id, asset_type, rated_flow, efficiency_params, vfd_status, scada_tag_group).
  - machinery_cycle(cycle_id, asset_id, timestamp, energyreqJ, pump_speed_rpm, blower_hz, gate_position_pct, DO, BOD_in, BOD_out, PFAS_in, PFAS_out).

- Modify C++ core:
  - Read telemetry from CSV/API exports.
  - Compute KER and ΔVt using real sensor values.
  - Populate machinery_cycle and workload_cycle consistently.

Target:

- Establish a continuous telemetry pipeline for at least one representative asset.

### Objective 2: Dashboard Views and Reporting

Actions:

- Implement views:
  - v_daily_ker_triad_per_asset: average K, E, max R per asset/day.
  - v_system_kwh_per_m3: energy per unit treated per plant/window.
  - v_carbon_footprint_summary: net CO2e per plant/window.
  - v_pfashazard_trend: PFAS risk index over time.

- Build reporting templates:
  - Markdown or HTML reports summarizing:
    - Energy savings vs baseline.
    - KER corridor compliance.
    - Net carbon trajectory.
    - PFAS and effluent quality trends.

Target:

- Support regular municipal reporting cycles with clear, quantifiable metrics.

### Objective 3: C++ System-Level Optimizer

Actions:

- Create library file:
  - cpp/optimization/system_workload_optimizer.cpp

- Implement:
  - Data loaders for system_node, system_asset, system_workload, system_risk.
  - MILP or heuristic solver for the workload distribution problem.
  - Output advisory plans to system_workload_plan table with KER summaries.

Target:

- Produce non-actuating workload plans that minimize energy and carbon under safe corridors.

### Objective 4: MCP Integration and AI Tools

Actions:

- Register views and optimization outputs as MCP tools:
  - tool_system_kwh_per_m3
  - tool_system_carbon_summary
  - tool_system_risk_status
  - tool_system_workload_plan

- Configure MCP server:
  - Read-only queries only.
  - Clear tool descriptions and schemas.

Target:

- Allow AI assistants to answer questions like:
  - "Which assets were most energy-efficient last week?"
  - "What is the net carbon for the primary plant this month?"
  - "List all cycles with K < 0.5 or R > 0.7 yesterday."

### Objective 5: Governance Tightening with ALN

Actions:

- Extend ALN particle:
  - Add system-level invariants over windows (K_W_sys, E_W_sys, R_W_sys).
  - Bind MCP tools and optimizer outputs to DID and corridors.

- Implement ALN checks:
  - C++ optimizer and SQL triggers must validate ALN invariants before accepting plans or metrics.

Target:

- Ensure all analysis, plans, and reports are provably aligned with governance rules and DID ownership.

## 6. Implementation Phases

Phase 1: Telemetry and schema hardening

- Integrate single asset telemetry.
- Validate constraints and triggers with real data.

Phase 2: Corridor calibration and digital twin

- Fit risk coordinate mappings and eco-impact weights.
- Build a twin model for BOD and PFAS processes.

Phase 3: Optimizer and dashboards

- Implement optimizer and views.
- Test closed-loop scheduling on historical datasets.

Phase 4: AI integration and municipal pilots

- Register MCP tools.
- Deploy AI-assisted dashboards and reporting for one plant or canal segment.
