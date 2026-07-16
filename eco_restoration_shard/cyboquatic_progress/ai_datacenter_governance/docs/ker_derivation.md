<!-- filename: eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/docs/ker_derivation.md
     purpose : Formal derivation of the K,E,R formulas used in AiDatacenterNode2026v1.
               Includes proof of monotonicity and boundedness. -->

# K,E,R Derivation for AI Cyboquatic Nodes

## 1. Lyapunov Residual Definition

Given the primary risk coordinates \(r_j\) from the mapping table, the scalar
residual at time \(t\) is
\[
V_t = \sum_{j \in \text{primary}} w_j \, r_j^2
\]
with weights as in the ALN particle (\(\sum w_j = 1\)).

## 2. Knowledge Factor K

The base knowledge factor reflects how well the system stays inside corridors
and whether it improves or worsens:
\[
K_{\text{base}} = 0.95 - 0.3\,V_t - 0.2\,\max(0,\,\Delta V_t)
\]
where \(\Delta V_t = V_t - V_{t-1}\).  The constants are chosen so that
- At the best‑case (\(V_t = 0, \Delta V_t \le 0\)), \(K_{\text{base}} = 0.95\).
- At the worst legal case (\(V_t = 0.13, \Delta V_t = 0.13\)),
  \(K_{\text{base}} = 0.95 - 0.3 \times 0.13 - 0.2 \times 0.13 = 0.885\), still
  within acceptable knowledge territory.

A secondary K‑boost of up to \(0.05\) is added based on throughput, utilisation,
and ERE, yielding
\[
K = \operatorname{clamp}_{[0,1]}(K_{\text{base}} + K_{\text{boost}}).
\]

## 3. Eco‑impact Factor E

E measures the net restorative potential:
\[
E = 0.95 - V_t - 0.15\,\max(0,\,\Delta V_t)
\]
Clamped to \([0,1]\).

## 4. Residual Risk R

R is the primary safety gate:
\[
R = V_t + \max(0,\,\Delta V_t)
\]
Clamped to \([0,1]\).  The ALN invariant enforces \(R \le 0.13\) for production
lanes.

## 5. Monotonicity and Boundedness Proof

- **Monotonicity in \(V_t\)**: All formulas are strictly decreasing in \(V_t\) for
  K and E, and strictly increasing for R.  Hence, any improvement that lowers
  the residual immediately raises K/E and lowers R.
- **Boundedness**: Because \(V_t \in [0,1]\) and \(\Delta V_t \in [-1,1]\),
  the expressions are bounded; clamping ensures outputs stay within \([0,1]\).
- **Always‑improve contract**: If a new daily shard produces \(\Delta V_t \le 0\),
  then \(K\) and \(E\) do not decrease and \(R\) does not increase, satisfying
  the forward‑only requirement of the Phoenix Hex Registry chain.
