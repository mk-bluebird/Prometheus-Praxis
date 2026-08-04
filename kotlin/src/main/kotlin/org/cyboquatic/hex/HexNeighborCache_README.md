# Kotlin In-Memory Hex Neighbor Cache

- `HexNeighborCache`:
  - Parses `phoenix_hex_registry.md` to map H3 indices to internal Long IDs.
  - Loads neighbour relationships from a CSV sidecar (or H3 bindings) into `Map<Long, List<Long>>`.
  - Provides `getNeighboursWithLST(cellId)` to fetch neighbours and their LST from `hex_thermal_recovery`.
  - Computes local discrete Laplacian of afternoon LST:
    - `Δ_LST(i) = deg(i) * LST_i - Σ_j LST_j`, used in the hex-anchor consistency equation and optimisation routines.

---

## 50. Grand challenge: multi‑objective optimal control for Phoenix cyboquatic eco‑restoration

Frame the Phoenix cyboquatic eco‑restoration network as a multi‑objective optimal control problem over hex‑cells and canal segments:

### State and control variables

- Hex‑cell states \(x_h(t)\):
  - LST anomaly \(\Delta T_h(t)\), green fraction \(g_h(t)\), canopy cover, albedo, heat‑stress metrics.
- Canal corridor states \(x_c(t)\):
  - PFAS concentration profiles, BOD, DO, Lyapunov exponents \(\lambda_{\max,c}(t)\), cold‑survival probabilities, KER triad per node.
- Controls:
  - \(u_{\text{green},h}(t)\): restoration actions (tree planting, cool pavement) per hex.
  - \(u_{\text{aer},c}(t)\): aeration intensity, routing decisions (FOG router), flow controls per canal segment.
  - \(u_{\text{workload}}(t)\): edge/cloud AI workload allocation, pump VFD setpoints.

### Objectives

1. **Hex‑anchor thermal equity**:
   - Minimise spatial variance of canopy temperature across hexes:
     \[
     J_{\text{thermal}} = \int_0^T \sum_h w_h \left(\Delta T_h(t) - \bar{\Delta T}(t)\right)^2 dt,
     \]
     where \(w_h\) weights vulnerable communities.

2. **Carbon negativity**:
   - Ensure cumulative KER eco‑impact is non‑positive:
     \[
     J_{\text{carbon}} = \int_0^T \sum_n ker_{e,n}(t) dt,
     \quad \text{with constraint } J_{\text{carbon}} \le 0.
     \]

3. **Safety (FOG routing)**:
   - Never route unsafe media to restoration canals:
     - Constrain FOG decisions so PFAS‑positive media with high FOG risk are routed to HOLD_TANK/treatment.
     - Minimise risk \(ker_r\) and PFAS stagnation, as in the ALN invariant.

### Multi‑objective optimal control problem

Find controls \(u(t) = (u_{\text{green},h}, u_{\text{aer},c}, u_{\text{workload}})\) over horizon \(T\) that:

- Minimise \(J_{\text{thermal}}\) and \(J_{\text{carbon}}\).  
- Satisfy safety constraints and KER invariants:
  - No unsafe routing.
  - KER triad bounds: \(ker_k \in [0,1]\), \(ker_e \le 0\), \(ker_r \in [0,1]\).
- Respect dynamics given by hex thermal equations (discrete Laplacian) and canal PDE/SDE models for PFAS/BOD.

This is high‑dimensional. To stay within ALN v2, SQL, Lua, Kotlin/Java, and C++—without blacklisted primitives—use a **decomposition strategy**:

### Decomposition into tractable sub‑problems

1. **Hex‑level thermal optimisation (Lua + Kotlin + SQL + C++)**

   - Sub‑problem: for each hex graph, solve discrete Laplace/Poisson consistency:
     \[
     L g = -f(\Delta T),
     \]
     to compute green‑fraction interventions that reduce LST while respecting local constraints.
   - Implementation:
     - Lua multigrid solver (`hex_laplace_multigrid.lua`) using adjacency cached in Kotlin (`HexNeighborCache`).
     - SQL stores `hex_thermal_recovery` and `hex_restoration_commitment`.
     - C++ workloads simulate impact of green changes on `deltaVt` and energy.

   - Objective: minimise local `Δ_LST` and enforce thermal equity by penalising spatial variance, solved in parallel per neighbourhood.

2. **Canal corridor PFAS/BOD control (SQL + Lua + C++ + ALN)**

   - Sub‑problem: corridor‑level control ensuring cold‑survival with PFAS reduction and carbon negativity.

   - Steps:
     - C++ simulators compute PFAS trajectories, BOD, aeration energy, VFD control, and measurement‑based \(ker_e\).  
     - SQL views (`pfas_lyap_residual`, `cold_survival_corridor`) maintain Lyapunov residuals, cold‑survival flags, and PFAS half‑life telemetry.  
     - Lua corridor analyser uses Lyapunov residuals and trends to classify corridors, route via FOG (`fog_router_trend.lua`), and plan restorative corridors (`hex_corridor_planning.lua`).  
     - ALN v2 invariants (PFAS stagnation penalty, KER bounds, hyperbolic discount) ensure governance constraints are satisfied.

   - Objective: minimise PFAS and risk \(ker_r\) while maintaining negative \(ker_e\), decomposed into corridor‑specific optimisation problems.

3. **Workload and energy allocation (Java/Kotlin + C++)**

   - Sub‑problem: AI workload splitting and pump control to reduce energy and carbon cost while preserving performance.

   - Steps:
     - Kotlin/Java services manage telemetry ingestion, Seq2Seq LST forecasting, and VFD‑adapted pump control.  
     - C++ models compute gradients `∂energyreqJ/∂Q` and edge/cloud trade‑offs using KER utility.  
     - SQL telemetry schemas track energy, CO₂ flux, and ker_e in real time.

   - Objective: local MPC problems per node balancing latency, carbon intensity, and risk.

4. **Governance and provenance layer (ALN v2 + SQL + Kotlin)**

   - Sub‑problem: KER triad governance, provenance, and audit.

   - Steps:
     - ALN entities (`GovernanceParticle`, `HexRestorationCommitment`, KER invariants) formally specify acceptable behaviour.  
     - SQL triggers and CTEs enforce KER constraints (`ker_e <= 0`, PFAS stagnation penalties, discounted eco‑impact).  
     - Kotlin services implement Merkle‑style provenance (without blacklisted algorithms) and eco‑audit logs.

   - Objective: ensure all sub‑controllers remain within governance boundaries and eco‑restoration commitments.

### Coordination to achieve Pareto‑optimal network state

Use a **hierarchical coordination**:

- Local controllers (hex, canal, workload) optimise their sub‑objectives subject to KER and safety constraints.  
- A higher‑level coordinator (e.g., a Kotlin/Java orchestrator) periodically:

  - Aggregates global metrics from SQL views (thermal equity, carbon totals, PFAS status).  
  - Adjusts weights \(w_{\text{thermal}}, w_{\text{carbon}}, w_{\text{safety}}\) in local objective functions.  
  - Uses ALN v2 governance rules to ensure:
    - Hex restoration commitments stay on target.  
    - Corridors reduce PFAS and remain cold‑survival without stagnation.  
    - FOG router never routes unsafe media to restoration canals (enforced via ALN/SQL invariants and Lua router logic).

Because each sub‑problem is tractable (convex or locally optimisable) and solved with tools available (ALN v2, SQL, Lua, Kotlin/Java, C++), this decomposition allows the entire Phoenix cyboquatic network to be driven toward a **Pareto‑optimal manifold**:

- Thermal equity maximised across hexes.  
- Net carbon negativity maintained by measurement‑based \(ker_e\) and discounting.  
- FOG router safety enforced with strong incentives against PFAS stagnation.

No blacklisted primitives (hashes or prohibited frameworks) are required: governance and provenance rely on generic digest and signature constructs; optimisation and control are implemented using standard numerical methods and database logic across the multi‑language architecture.
