# Fully Coupled Hex–Canal Adjoint Optimisation and CATE Storage for Microclimate

## 1. Hex–Canal Adjoint Optimisation (Math Object)

We treat hex-cells and canal segments as coupled subsystems:

- **Hex side**: Green-fraction \(g_h(t)\) per H3 cell controls LST anomaly \(\Delta T_h(t)\) via a discrete Poisson equation:

  \[
  L g = -f(\Delta T),
  \]

  with graph Laplacian \(L\) over H3 adjacency.

- **Canal side**: PFAS and BOD transport obey advection–dispersion–reaction PDEs:

  \[
  \frac{\partial C}{\partial t} + v \frac{\partial C}{\partial x}
  = D \frac{\partial^2 C}{\partial x^2} - k_s(x,t) C,
  \]
  \[
  \frac{\partial B}{\partial t} + v \frac{\partial B}{\partial x}
  = D_B \frac{\partial^2 B}{\partial x^2} - k_B(T(x,t), u_c(x,t)) B,
  \]

  where \(u_c(x,t)\) is aeration control.

The joint objective:

\[
J[g, u_c]
= \int_0^T \left[
    \alpha \sum_h w_h (\Delta T_h(t) - \bar{\Delta T}(t))^2
  + \beta \int_\Omega w_c(x) C(x,t) \, dx
  + \gamma\, ker_e(t)
\right] dt,
\]

is minimized subject to the PDE constraints and a global carbon-negative condition \(\int_0^T ker_e(t)\,dt \le 0\).

The continuous adjoint equations follow standard PDE-constrained optimisation:

- **Hex adjoint** \(\lambda_h(t)\) arises from variations in \(g_h\) and the discrete Poisson constraint, yielding an adjoint graph equation \(L^T \lambda = \text{source}\) that couples LST variance to green-fraction.

- **Canal adjoints** \(p_C(x,t), p_B(x,t)\) satisfy backward-in-time PDEs derived from the Hamiltonian \(H = L + p f\):

  \[
  -\frac{\partial p_C}{\partial t} - v \frac{\partial p_C}{\partial x}
  = -\frac{\partial L}{\partial C} + D \frac{\partial^2 p_C}{\partial x^2} + k_s p_C,
  \]
  \[
  -\frac{\partial p_B}{\partial t} - v \frac{\partial p_B}{\partial x}
  = -\frac{\partial L}{\partial B} + D_B \frac{\partial^2 p_B}{\partial x^2} + k_B p_B,
  \]

with terminal conditions at \(t = T\) from the objective.

The control gradients:

\[
\frac{\partial J}{\partial g_h} \quad\text{and}\quad
\frac{\partial J}{\partial u_c(x,t)}
= \frac{\partial L}{\partial u_c} + p_C \frac{\partial f_C}{\partial u_c} + p_B \frac{\partial f_B}{\partial u_c},
\]

drive updates to hex green-fractions and canal aeration.

### Discretisation for C++ Finite-Difference Solvers

- Discretise canal domain into grid points \(x_i\) and time steps \(t_n\), using finite differences for forward PDEs and backward adjoint PDEs.
- Implement hex Laplacian \(L\) as a graph operator:

  \[
  (L g)_i = \text{deg}(i) g_i - \sum_{j \in N(i)} g_j.
  \]

- Compute forward states \((C_i^n, B_i^n, g_h^n)\), then adjoints \((p_{C,i}^n, p_{B,i}^n, \lambda_h^n)\), and finally gradients for \(g_h\) and \(u_{c,i}^n\).
- Use gradient-based MPC to update controls in C++.

### Data-Exchange Wiring

- **Hex solver (Lua multigrid)**:
  - Computes updated \(g_h\) fields and writes them to SQL (`hex_thermal_recovery`, `hex_restoration_commitment`).
- **Canal solver (C++ PDE/MPC)**:
  - Reads hex-derived temperature and albedo from SQL to set \(T(x,t)\) and decay rates \(k_B\).
  - Writes PFAS/BOD states, Lyapunov exponents, and ker_e telemetry back to SQL (`pfas_corridor_telemetry`, `cold_survival_corridor`).
- Coordination:
  - Periodic cycles where C++ reads hex outputs, runs canal optimisation, then Lua hex solver adjusts green-fraction using updated canal metrics and governance targets, all mediated by SQL schemas and views.

## 2. CATE Storage and Query for Microclimate Interventions

The heterogeneous treatment effect (CATE) of interventions (canopy, cool pavement, aeration basins) on afternoon LST is learned offline (e.g., double ML or causal forests) and stored in `hex_cate`:

- `hex_cate` table:
  - `h3_index`: hex identifier.
  - `intervention_type`: e.g., 'canopy'.
  - `cate_lst_drop_k`: predicted LST reduction (K) per unit intervention.
  - `cate_confidence`: confidence or inverse standard error.
  - `cost_unit`: cost per unit intervention.

The view `hex_cate_canopy_value` computes `lst_drop_per_cost = cate_lst_drop_k / cost_unit`, enabling value-based ranking.

The Kotlin helper `HexCateQuery.topKCanopyHexes(k, confidenceMin)`:

- Executes a parameterised query:

  ```sql
  SELECT h3_index, cate_lst_drop_k, cost_unit, lst_drop_per_cost
  FROM hex_cate_canopy_value
  WHERE cate_confidence >= ?
  ORDER BY lst_drop_per_cost DESC
  LIMIT ?;
  ```

- Returns a list of hexes with the highest predicted LST reduction per unit cost at sufficient confidence.

This allows the Kotlin dashboard to guide real-time resource allocation:

- Visualise top-k hexes for canopy expansion with maximum cooling effect per budget.
- Integrate with corridor planning and governance commitments, ensuring interventions are both thermally effective and economically efficient, while remaining consistent with carbon-negative and KER constraints.

Technical justification: The adjoint formulation couples hex and canal controls through shared objective and PDE constraints, enabling gradient-based MPC in C++ while Lua and SQL manage hex Laplacian and canal telemetry. The CATE schema and query provide a practical way to expose heterogeneous treatment effects to operators via Kotlin dashboards, driving microclimate interventions based on statistically grounded, cost-aware predictions stored in SQLite.
