# K,E,R Derivation for AiDatacenterNode2026v1

Given risk coordinates \(r_j\), the Lyapunov residual is
\[
V_t = \sum_j w_j r_j^2
\]
with weights in `AiDatacenterNode2026v1.aln`.[file:91]

Let \(\Delta V_t = V_t - V_{t-1}\).

- Knowledge factor:
  \[
  K = \operatorname{clamp}_{[0,1]}(0.95 - 0.30 V_t - 0.10 \max(0, \Delta V_t) + K_{\text{boost}})
  \]
  where \(K_{\text{boost}}\) is defined in the 10‑axis mapping document.[file:92]

- Eco‑impact factor:
  \[
  E = \operatorname{clamp}_{[0,1]}(0.93 - 0.25 V_t)
  \].[file:91]

- Residual risk:
  \[
  R = \operatorname{clamp}_{[0,1]}(0.12 + 0.40 V_t + 0.20 \max(0, \Delta V_t))
  \].[file:92]

Monotonicity:

- \(\partial K / \partial V_t < 0\), \(\partial E / \partial V_t < 0\), \(\partial R / \partial V_t > 0\); thus higher residual always lowers K,E and raises R.[file:91]
- If \(\Delta V_t \le 0\), then R cannot increase and K,E cannot decrease aside from clamping, satisfying the always‑improve requirement for Phoenix hex chains.[file:91][file:15]
