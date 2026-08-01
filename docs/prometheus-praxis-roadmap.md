# Prometheus-Praxis Module Deployment Roadmap (Phoenix Focus)

## 1. Module Overview (Phoenix Clusters)

| Module             | Cluster               | Current assets (ALN/Rust/Lua)                                                 | Safety invariants in place                                                                                 | Missing invariants / docs                                                                                  | Phoenix nodes (targets)                                                                                  | Deployment mode  | Priority |
|--------------------|-----------------------|-------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------|------------------|----------|
| Kairos-Executor    | Governance/Scheduling | ALN: prometheus-praxis-kairos-executor.v1.aln; Rust: core + KER + Kani invariants | RoH lane profiles; Lyapunov non-increase for windows; governance bindings (Rights, PSAT, Veritas/Janus)   | Must-Stop proofs when RoH/KER/Lyapunov violated; ISO/IEC 42001 role/risk metadata                         | Canal pumps; MAR vault scheduling; cyboquatic mission windows                                            | Planning-only    | 1        |
| Physis-Curator     | Eco/City              | ALN: physis-curator.v1.aln; Rust: Physis planner                             | Soil and aquifer Lyapunov non-increase; PSAT required for nanoswarms/heavy machinery                      | Field-calibrated Vsoil/Vaquifer for Phoenix; eco-restoration Annex IV alignment                           | Canal banks; MAR basins; brownfields; storm channels                                                    | Planning-only    | 1        |
| Hygeia-Orchestrator| Healthcare/Cybernetics| ALN: hygeia-orchestrator.v1.aln; Rust: health orchestrator                   | Clinical envelopes (lifeforce, RoH); PSAT neurorights binding; non-rollback of capabilities               | Device-specific PSAT/KER mapping; bindings to FDA/clinical trial protocols                                | Clinic networks; nanoswarm controllers; BCIs                                                            | Research-only    | 2        |
| Synthsis-Nexus     | Healthcare/Cybernetics| ALN: synthesis-nexus.v1.aln; Rust: synthesis/OTA fabric                      | Monotone capability vectors; PSAT-bound OTA evolution; consent ledger/AugFingerprint binding              | Normative capability vector decomposition with Kani proofs; device-class OTA envelopes                   | Augmentation OTAs; BCI firmware; AugFingerprint corridors                                              | Research-only    | 2        |
| Nomos-Enforcer     | Governance/Scheduling | ALN: nomos-enforcer.v1.aln; Rust: treaty/rights gates                         | Policy-to-action refinement; rights/treaty envelopes enforced; lane-wise risk classification              | Machine-readable AI Act risk mapping per domain; expanded Kani harnesses for rights-risk Stop decisions   | Eco plans; city upgrades; healthcare programs; BioPay pilots                                            | Planning-only    | 1        |
| Aletheia-Verifier  | Governance/Scheduling | ALN: aletheia-verifier.v1.aln; Rust: provenance anchor                        | Veritas/Janus anchoring for decisions; latency ceilings; no unanchored Allow/Derate                      | Formal latency budgets per corridor; audit indexing schemas                                               | Canal regime changes; cyboquatic missions; clinic procedures; EcoNet rewards                            | Diagnostic-only  | 1        |
| Demos-Synaxis      | Collective/Innovation | ALN: demos-synaxis.v1.aln; Rust: collective feed                              | Community inputs cannot relax RoH/Tsafe/neurorights; consent/anonymization invariants; signals as KER evidence | Phoenix Nosphere/Gemeinschaft/Wachstum ingestion; quantitative models for priority alignment and equity gaps | Water/heat corridors; canal campaigns; augmentation rollouts                                            | Diagnostic-only  | 2        |
| Pros-Optima        | Eco/City              | ALN: poros-optima.v1.aln; Rust: optimizer                                     | Seven-capital corridors; KER/Lyapunov residual bounds; treaty corridor respect                           | Fielded multi-capital models; robustness tests under real load                                           | Canal ops; FOG channels; district energy/water grids                                                    | Planning-only    | 1        |
| Techn-Incubator    | Collective/Innovation | ALN: techne-incubator.v1.aln; Rust: pilots lifecycle                          | Pilot states (lab/micro/district/retired); ProofOfResearch + Kani invariants; monotone safety envelopes  | Concrete Phoenix pilot registry; post-market surveillance hooks                                          | Canal/cyboquatic pilots; clinic nanoswarm pilots; EcoNet/Credits pilots                                  | Planning-only    | 1        |
| Chthnios-Monitor   | Eco/City              | ALN: chthonios-monitor.v1.aln; Rust: subsurface monitor                       | Subsurface Lyapunov non-increase; no scheduling when subsurface state poorly known; missing data treated as risk | Full Phoenix subsurface sensor deployment; mapping subsurface health to macro RoH/KER                    | Canal bank probes; MAR vault aquifer sensors                                                            | Diagnostic-only  | 1        |
| Phos-Comms         | Governance/Scheduling | ALN: phaos-comms.v1.aln; Rust: PQC-secured comms                              | Forbidden primitives; PQC whitelist; readonly governance channels for actuators                          | PQC scheme assignments per corridor; latency/reliability SLAs                                           | Telemetry from canals; cyboquatic machinery; clinic BCIs; city controllers                              | Diagnostic-only  | 1        |
| Stsis-Guard        | Governance/Scheduling | ALN: stasis-guard.v1.aln; Rust: guards + Kani invariants                      | Allow/Derate/Stop bound to RoH/Tsafe/Lyapunov/neurorights/treaty; non-rollback; RoH ceiling 0.30         | Domain-specific guard policies (eco/health/cybernetics/payments); expanded Kani harnesses               | All pilots (eco, cyboquatic, health, payments); city controllers                                        | Planning-only    | 1        |

## 2. Phoenix Pilots Mapped to Prometheus-Praxis Modules

| Pilot ID | Pilot name                                         | Primary modules touched                                                                  | Must-pass invariants (examples)                                                                                       | Key ALN artifacts                                                                                                       | Key Rust crates                                                                                                   | Key Lua modules                                                                                       | Phoenix nodes / scope                                                                                                             | Priority |
|----------|----------------------------------------------------|------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------|----------|
| P1       | Phoenix canal eco regime (non-cyboquatic)          | Kairos-Executor; Physis-Curator; Chthnios-Monitor; Pros-Optima; Stsis-Guard; Aletheia-Verifier; Nomos-Enforcer | RoH ≤ 0.30 for canal corridor; soil/aquifer Lyapunov non-increase; no scheduling when subsurface state unknown; treaty corridors respected | prometheus-praxis-kairos-executor.v1.aln; physis-curator.v1.aln; chthonios-monitor.v1.aln; poros-optima.v1.aln; stasis-guard.v1.aln; aletheia-verifier.v1.aln; nomos-enforcer.v1.aln | crates/prometheuspraxis; crates/prometheuspraxisker; crates/prometheuspraxischthonios; crates/prometheuspraxisoptimizer; crates/prometheuspraxisguards; crates/prometheuspraxisprovenance; crates/prometheuspraxistreatygates | runtime/lua/prometheus-praxis/ecoindexdiagnostics.lua (planned); governance gate if diagnostics exposed to agents | One canal segment: pump regimes, soil/aquifer sensors, temperature and PFAS corridors; planning-only, non-actuating | 1        |
| P2       | Cyboquatic remediation missions in Phoenix canals  | Kairos-Executor; Physis-Curator; Chthnios-Monitor; Pros-Optima; Stsis-Guard; Aletheia-Verifier; Nomos-Enforcer; Demos-Synaxis | RoH ≤ 0.30 for cyboquatic corridor; Lyapunov non-increase for water/soil; PSAT binding for heavy machinery; FPIC & protected-reach gates; community inputs cannot relax safety envelopes | physis-curator.v1.aln; chthonios-monitor.v1.aln; poros-optima.v1.aln; stasis-guard.v1.aln; aletheia-verifier.v1.aln; nomos-enforcer.v1.aln; demos-synaxis.v1.aln; ecosafety governance ALN shards | crates/prometheuspraxis; crates/prometheuspraxisker; crates/prometheuspraxischthonios; crates/prometheuspraxisoptimizer; crates/prometheuspraxisguards; crates/prometheuspraxisprovenance; crates/prometheuspraxistreatygates; crates/prometheuspraxiscollectivefeed | runtime/lua/prometheus-praxis/ecoindexdiagnostics.lua; runtime/lua/prometheus-praxis/guard_ecogovernancegate.lua | Two canal segments with cyboquatic machinery (robots/rovers) performing PFAS/E. coli remediation; diagnostics + planning-only; hardware in separate stacks | 1        |
| P3       | FOG channels and trash routing eco-ops             | Physis-Curator; Pros-Optima; Kairos-Executor; Stsis-Guard; Aletheia-Verifier; Nomos-Enforcer | RoH and KER corridors for FOG/trash flows; Lyapunov stability for FOG channels; treaty constraints for waste routing; non-rollback of eco protections | physis-curator.v1.aln (FOG/cleanup); poros-optima.v1.aln; stasis-guard.v1.aln; aletheia-verifier.v1.aln; nomos-enforcer.v1.aln; nanorobotics.obs.metrics.schema.v1.aln | crates/prometheuspraxis; crates/prometheuspraxisker; crates/prometheuspraxisoptimizer; crates/prometheuspraxisguards; crates/prometheuspraxisobservability; crates/prometheuspraxisprovenance; crates/prometheuspraxistreatygates | lua/prometheus-praxis/fog_workload_diagnostics.lua (planned)                                                | FOG interceptors, trash-routing controllers, canyon windnets for one district; eco workload analytics + planning-only | 2        |
| P4       | Clinic-level nanoswarm care band                   | Hygeia-Orchestrator; Synthsis-Nexus; Stsis-Guard; Nomos-Enforcer; Aletheia-Verifier; Phos-Comms | Clinical envelopes with lifeforce floors and RoH ceilings; PSAT/neurorights binding for nanoswarm/BCI actions; monotone capability evolution; secure PQC telemetry; non-rollback of host capabilities | hygeia-orchestrator.v1.aln; synthesis-nexus.v1.aln; stasis-guard.v1.aln; nomos-enforcer.v1.aln; aletheia-verifier.v1.aln; phaos-comms.v1.aln; neurorights/treaty ALN shards | crates/prometheuspraxishealth; crates/prometheuspraxissynthesis; crates/prometheuspraxisguards; crates/prometheuspraxistreatygates; crates/prometheuspraxisprovenance; crates/prometheuspraxiscomms; crates/prometheuspraxisker | lua/prometheus-praxis/clinical_governance_diagnostics.lua (future, gated)                                   | Selected clinics performing nanoswarm rehab and augmentation updates; planning-only and envelope orchestration; devices enforce hardware constraints | 2        |
| P5       | EcoNet rewards and Credits overlay                 | Nomos-Enforcer; Demos-Synaxis; Techn-Incubator; Pros-Optima; Stsis-Guard; Aletheia-Verifier; Phos-Comms | Non-plutocratic, public-good-only reward routing; no exclusion from basics; no scoring from inner mental state; RoH ≤ 0.30; neurorights-safe EcoNet; community inputs cannot be coerced | techne-incubator.v1.aln; poros-optima.v1.aln (eco reward corridors); stasis-guard.v1.aln; nomos-enforcer.v1.aln; aletheia-verifier.v1.aln; demos-synaxis.v1.aln; EcoNet/EcoGrant ALN shards | crates/prometheuspraxiseconet; crates/prometheuspraxis; crates/prometheuspraxisker; crates/prometheuspraxisguards; crates/prometheuspraxisprovenance; crates/prometheuspraxistreatygates; crates/prometheuspraxiscollectivefeed; crates/prometheuspraxispilots | lua/prometheus-praxis/econet_reward_introspection.lua (planned, governance-gated; no direct settlement)      | Phoenix residents and workers performing eco actions (recycling, canal restoration, civic reporting, cyboquatic oversight) rewarded via Credits/EcoNet; non-actuating, planning and scoring only | 2        |

# Canal Surcharge and Blast-Radius Monitoring  
## Prometheus-Praxis Research Dossier and Upgrade Roadmap

This document enumerates research objects for canal surcharge and blast-radius monitoring in Prometheus-Praxis, defines upgrade pathways for real machinery and dashboards, and specifies math and tooling to improve AI-chat compatibility and future code outputs.

## 1. Existing Research Objects and Roles

### 1.1 Hex Registry, KER–Lyapunov Corridor, Blast-Radius

**Research objects:**

1. `phoenix_hex_registry` (conceptual registry)  
   - Purpose: Index Phoenix urban/canal hexes as spatial anchors for risk planes (hydraulics, energy, topology, biodiversity).  
   - Role: Each hex holds time-series risk coordinates, enabling spatial Lyapunov residuals \(V_h(w)\) and corridor design.

2. KER–Lyapunov coupling  
   - Composite score:
     \[
       s_t = K_t \cdot E_t - R_t
     \]
   - Lyapunov residual:
     \[
       V_t = \sum_j w_j r_{t,j}^2,\quad w_j \ge 0
     \]
   - Corridor constraint:
     \[
       V_{t+1} - V_t \le -\alpha s_t,\quad \alpha > 0
     \]
     ensuring residual non-increase under safe steps.

3. Hydraulic blast-radius decay model (PDE-based)  
   - Surcharge energy density field \(E(\mathbf{x}, t)\) around canal nodes, modeled via diffusion–advection–reaction:
     \[
       \frac{\partial E}{\partial t} = D \nabla^2 E - \mathbf{v} \cdot \nabla E - \lambda E + S(\mathbf{x}, t)
     \]
   - Dimensionless canal-surcharge numbers (see §3.1) link hydraulic parameters and structural limits.

4. Multi-plane risk weights and non-offsettable planes  
   - Lyapunov residual per hex:
     \[
       V_h(w) = w_h r_{h,\text{hyd}}^2 + w_e r_{h,\text{ene}}^2 + w_t r_{h,\text{top}}^2 + w_b r_{h,\text{bio}}^2 + \dots
     \]
   - Optimization: choose \(w\) via minimax over hexes and windows:
     \[
       \min_{w \in \mathcal{W}} \max_{h \in \mathcal{H}} \bar{V}_h(w),\quad \bar{V}_h(w) = \max_{t \in W} V_{h,t}(w)
     \]
   - Non-offsettable planes (e.g., biodiversity) must respect hard corridors regardless of aggregate \(V_h(w)\).

5. Hex-anchor consistency (SQLite invariants)  
   - DID-bound registry entries with modular checksums over hex coordinates and DID.  
   - Enforced via `CHECK` and triggers so any attempt to change hex identifiers or domains without updating the checksum aborts.

Together, these objects form a spine: hex-indexed risk planes, Lyapunov residual semantics, KER triads, and PDE-based blast-radius fields grounded in urban drainage and restoration theory.

### 1.2 MCP / SQLite Knowledge Graph

**Research objects:**

1. `mcp_repo`, `mcp_file`, `mcp_tool`, `mcp_endpoint`  
   - Normalized SQLite schema for repositories, files, and tools, with governance metadata: `roleband`, `primaryplane`, `lanedefault`, `resourcemode`, `ker_hint`.  
   - Allows registration of canal surcharge simulators, blast-radius SQL views, and telemetry tools as discoverable MCP tools.

2. Discovery views (`v_mcp_tools`)  
   - Joins repo, file, tool metadata into a flat catalog.  
   - Enables queries such as “all hydraulic blast-radius tools in PROD lane for Phoenix hexes”.

This graph is the backbone for wiring canal surcharge and blast-radius tools into Prometheus-Praxis in a governed, AI-chat-friendly way.

## 2. Missing Invariants, Docs, and Upgrade Pathways

### 2.1 Missing Elements

1. Operational PDE models bound to Phoenix canal assets  
   - Need a discrete mesh over canal segments and adjacent hexes.  
   - Need calibrated \(D\), \(\mathbf{v}\), \(\lambda\), and source terms from Phoenix/SRP hydraulics.  
   - Need C++/SQL solvers integrated with telemetry.

2. Unified telemetry schema for pumps, gates, sensors  
   - Required fields: flow, level, gate position, pump state, structural sensors.  
   - Linkage to `phoenix_hex_registry`, canal-node IDs, and blast-radius risk coordinates.

3. Probabilistic surcharge risk functions linked to Lyapunov residuals  
   - Need mappings from overflow/surcharge probabilities to hydraulic risk coordinates \(r_{\text{hyd}}\).  
   - Need integration with deterministic PDE fields and KER triads.

4. Dashboard-grade views and MCP tools  
   - Need canonical SQL views for daily and storm-event blast-radius indices.  
   - Need MCP tools wrapping these views with clear input/output schemas.

5. Formal non-actuating alert design and routing  
   - Need `surcharge_alert` schema (severity, message, recommended actions, acknowledgements).  
   - Need ALN v2 particles constraining auto-generated alerts and recommendations.

## 3. New Functions and Math to Advance Work

### 3.1 Dimensionless Canal-Surcharge Numbers

Define:

1. Hydraulic surcharge number:
   \[
     S_c = \frac{Q L}{A c}
   \]
   - \(Q\): discharge  
   - \(L\): characteristic canal length  
   - \(A\): cross-sectional area  
   - \(c\): structural capacity coefficient

2. Energy blast-radius number:
   \[
     B_e = \frac{E_{\text{peak}}}{E_{\text{crit}}}
   \]
   - \(E_{\text{peak}}\): peak surcharge energy  
   - \(E_{\text{crit}}\): energy threshold for likely damage or ecological harm

Use these to define corridors:

- Green: \(B_e \le 0.6\) and \(S_c \le S_{c,\text{design}}\)  
- Amber: \(0.6 < B_e \le 1.0\) or \(S_c\) near design limit  
- Red: \(B_e > 1.0\) or \(S_c > S_{c,\text{design}}\)

### 3.2 Lyapunov Residual and KER Windows

For hex \(h\) over window \(W\):

- Residual:
  \[
    V_{h,W}(w) = \sum_j w_j \max_{t \in W} r_{h,j}(t)^2
  \]
- Corridor inequality:
  \[
    V_{h,W}(w) \le V_{\text{max}},\quad r_{h,\text{hyd}}(t) \le r_{\text{hyd,max}} \;\forall t \in W
  \]
- KER window triad:
  \[
    K_W = \frac{\#\{ t \in W \mid V_{h,t+1} \le V_{h,t} \}}{|W|}
  \]
  \[
    E_W = g(\text{energy usage}, \text{eco-benefit})
  \]
  \[
    R_W = \max_{t \in W} \max_j r_{h,j}(t)
  \]

These formulas should be implemented in SQL views and C++ utilities (e.g., `v_hex_ker_compliance`, `cpp/simulation/hex_ker_metrics.cpp`).

### 3.3 Probabilistic Overflow and Storage

Define inflow/outflow discharge and storage adequacy:

- Inflow \(Q_i\), outflow \(Q_o\), runoff volume \(V_r\).  
- Required detention storage:
  \[
    V_s = R_s(Q_o/Q_i, \text{storm type}) \cdot V_r
  \]

Storage adequacy index:
  \[
    I_{\text{storage}} = \frac{V_s}{V_r}
  \]

Blast-radius risk enhancement when \(I_{\text{storage}} < 1\) under design storms:

- If \(I_{\text{storage}} < 1\), mark hexes as structurally vulnerable and raise hydraulic risk coordinate.

Probability of surcharge per hex-node pair:
  \[
    P_{\text{surcharge}}(h,n) = \sigma\left(\beta_0 + \beta_1 E_n^{\text{max}} + \beta_2 Q_n^{\text{peak}} + \beta_3 H_n^{\text{max}} + \beta_4 I_{\text{storage}}\right)
  \]
Hydraulic risk:
  \[
    r_{h,\text{hyd}} = \min\left(1,\; \gamma_1 P_{\text{surcharge}}(h,n) + \gamma_2 P_{\text{overflow}}(h,n)\right)
  \]

## 4. New Research Objects and Tooling

### RO-1: Phoenix Canal Surcharge PDE Simulator

- File: `cpp/simulation/canal_surcharge_pde.cpp`  
- Responsibilities:  
  - Build discrete mesh over canal nodes and adjacent hexes.  
  - Integrate telemetry feeds (flow, level, gate states).  
  - Step PDE, compute \(E_i(t)\), \(B_e\), and \(BR_i\).  
  - Expose results via SQLite tables (`canal_pde_state`, `hex_blast_radius_state`).

### RO-2: Hydraulic Risk SQL Views

- Views:  
  - `v_hex_hydraulic_risk` (computes \(r_{h,\text{hyd}}\) from PDE state, storage indices, and probability models).  
  - `v_hex_ker_compliance` (computes KER triads over windows and flags corridor violations).

### RO-3: Dashboard and Alert Schema

- Tables:  
  - `hex_blast_radius_daily` and `hex_blast_radius_storm` for daily and event-based metrics.  
  - `surcharge_alert` for advisory alerts with severity and recommended actions.

- MCP tools:  
  - `tool_blast_radius_daily`, `tool_blast_radius_storm`, `tool_surcharge_alerts`, all READONLY.

### RO-4: ALN Governance Particle for Surcharge

- Particle: `cyboquatic.surcharge.blast_radius.aln`  
- Responsibilities:  
  - Bind to DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`.  
  - Define corridors and invariants for hydraulics, topology, biodiversity.  
  - Constrain alerts and recommendations to non-actuating advisory templates.

## 5. Wiring and Production-Readiness Actions

1. Ensure directory paths:  
   - Simulation: `cpp/simulation/`  
   - Eco models: `cpp/eco_restoration/`  
   - Tools: `cpp/tools/`  
   - SQL schemas: `sql/`  
   - ALN particles: `aln/`.

2. Wire MCP registry:  
   - Register surcharge simulators and views in `mcp_tool` and `mcp_endpoint`.  
   - Set `resourcemode = 'READONLY'` for all blast-radius and alert tools.

3. Enhance structs/configs:  
   - C++ structs for canal nodes, hexes, and telemetry with explicit units and ranges.  
   - Configuration files for Phoenix hydraulics (design storms, structural limits) under `cpp/tools/config/`.

4. Improve AI-chat usability:  
   - Provide JSON-like schemas for all MCP tools.  
   - Maintain markdown docs generated from DB schemas and ALN particles as part of `docs/phoenix_canal_surcharge.md`.

Technical justification:  
The corrected document preserves all core functionality while removing citation markers, deduplicating sections, and enriching the roadmap with concrete math formulas, structuring of research objects, and wiring plans for C++/SQL/ALN modules within Prometheus-Praxis. It clarifies how Phoenix-specific canal surcharge and blast-radius monitoring can progress from conceptual KER–Lyapunov corridors to production-ready telemetry simulators, dashboards, and governance tools, improving future AI-chat integrations and code generation quality without rollbacks or disallowed primitives.
