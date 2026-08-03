# KER and Lyapunov Tuning Research Note

## 1. How K, E, R Triads Are Currently Used

EcoNet’s ecosafety spine represents system health with a risk vector \(r_t = (r_{t,1},\dots,r_{t,n})\) where each coordinate \(r_{t,j} \in [0,1]\) and per‑coordinate safegoldhard corridors enforce upper bounds on hydraulic, energy, topology, biodiversity, calibration, and uncertainty planes.[94] A Lyapunov residual
\[
V_t = \sum_j w_j r_{t,j}^2,\quad w_j \ge 0
\]
is maintained, with **safestep** enforcing \(V_{t+1} \le V_t\) plus rejection when any \(r_{t,j}>1\).[94]

KER window semantics:

- \(K\) (Knowledge) = fraction of Lyapunov‑safe steps in a window.
- \(R = \max_j r_{t,j}\) (plus uncertainty); \(E = 1 - R\) as eco‑impact margin.
- Production gates like \(K \ge 0.90, E \ge 0.90, R \le 0.13\) define admissible lanes.[94]

The composite score \(s = k\cdot e - r\) is used as a scalar diagnostic: when \(s>0\), corridors are designed so risk coordinates move inward and Lyapunov residual decreases. This is encoded as an invariant:
\[
V_{t+1} - V_t \le -\alpha s_t,\quad s_t = k_t e_t - r_t,\ \alpha>0,
\]
with \(s_t\) lower‑bounded by a change in risk \( \Delta R_t\) via \(s_t = \beta (k_t e_t - r_t) \le \Delta R_t\).[94] CI (`econet-ci-lyapunov`) replays Phoenix shards and asserts that for production windows with \(s_t>0\), measured \(V_{t+1}-V_t\) does not exceed \(-\alpha s_t\).[94]

## 2. How C++ Simulations Can Tune Weights and Corridors

C++ eco simulations (PFAS fate corridors, hydraulic blast‑radius PDEs, workload energy kernels) can act as controlled “safestep approximators”: given proposed plane weights \(w_j\) and corridor parameters, they replay shard windows (Phoenix canals, MAR vaults, cyboquatic workloads) and compute:

- Risk trajectories \(r_{t,j}\) per plane.
- Lyapunov residuals \(V_t(w)\) and their increments \(V_{t+1}-V_t\).
- Window KER triads \((K_t,E_t,R_t)\) and scalars \(s_t = k_t e_t - r_t\).

Tuning loop:

1. **Weight calibration:** For a candidate weight vector \(w\), compute worst‑case residual per hex
   \[
   \bar{V}_h(w) = \max_{t\in \text{season}} V_{h,t}(w)
   \]
   and solve the minimax problem
   \[
   \min_{w \in \mathcal{W}} \max_{h} \bar{V}_h(w)
   \]
   subject to non‑offsettable constraints for high‑hazard planes (carbon, biodiversity, neurorights).[94] C++ simulations provide the \(V_{h,t}(w)\) values needed; outer optimisation can be done via grid search or gradient‑free methods.

2. **Corridor threshold refinement:** For each plane, sweep candidate corridor thresholds (e.g. allowed \(r_{\text{blast}}\), PFAS mass bands) and re‑run simulations. Reject any configuration where:
   - Empirical \(K\) falls below target, or
   - \(V_{t+1}-V_t\) violates the bound \( \le -\alpha s_t\) when \(s_t>0\).[94]

3. **Plane‑specific stress and coupling:** Hydraulics C++ modules approximate blast‑radius PDEs, yielding dimensionless canal‑surcharge numbers that can be mapped into hydraulic/topology risk coordinates and corridors.[94] PFAS C++ modules simulate sorbed fraction and cold‑survival; material modules feed eco‑impact factors from biodegradability curves.[94] These engines turn abstract corridor definitions into numerically tested bands.

By embedding these checks directly in C++ simulation harnesses (e.g., `multiplane_risk_harness.cpp`), corridor and weight updates can be supplied with machine‑verifiable evidence that KER and Lyapunov invariants hold over realistic, hex‑anchored shards before any governance promotion.
