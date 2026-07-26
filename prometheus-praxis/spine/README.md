<!-- filename: prometheus-praxis/spine/README.md -->

# Prometheus‑Praxis Spine

This crate implements the **KER–Lyapunov foundational spine** for the Prometheus‑Praxis eco‑machine.[file:56][file:84]

It provides a minimal, non‑negotiable core:

- **RiskCoord** – single normalized risk coordinate \(r_j \in [0,1]\) with plane and metric labels.[file:56]
- **RiskVector** – per‑plane aggregation of coordinates, enforcing plane consistency and bounds.[file:56]
- **LyapunovWeights** – per‑plane weights \(w_j\) with `non_offsettable` flags for constitutionally locked planes.[file:84]
- **Residual** – Lyapunov energy \(V_t = \sum_j w_j r_j^2\), computed over all planes.[file:56]
- **SafeStepGate** – gatekeeper enforcing:
  - \(r_j \in [0,1]\) for every coordinate,
  - corridor grammar (no HARD band for new builds),
  - Lyapunov safe step \(V_{t+1} \le V_t\),
  - non‑offsettable plane invariants when locked.[file:56][file:84]
- **Corridor mapping helpers** – banded mappings for harmful and beneficial metrics into \([0,1]\).[file:56]

Domain crates (hydrology, topology, microplastics, neurorights, Tree‑of‑Life) plug into this spine by producing `RiskVector`s and `LyapunovWeight`s; the residual kernel and gate do not need to change.[file:84]

## Quick start

1. Add the spine crate to your workspace:

- In the root `Cargo.toml`, include `prometheus-praxis-spine` as a member.
- In a domain crate (e.g. `plane_hydrology`), depend on `prometheus-praxis-spine`.

2. Construct risk coordinates and vectors:

- Use `RiskCoord::from_raw` with a metric‑specific mapping closure (harmful or beneficial corridors).
- Group coordinates per plane with `RiskVector::new`.

3. Configure Lyapunov weights:

- Create `LyapunovWeight` values per plane, marking critical planes as `non_offsettable`.
- Build `LyapunovWeights` and pass them to `Residual::compute`.

4. Gate state transitions:

- Before committing a new state, call `SafeStepGate::evaluate_step` with current and next vectors.
- Reject transitions when the gate returns `SafeStepDecision::Rejected`.

This crate is designed to remain stable over decades; extensions occur in domain crates and ALN schemas, while the spine invariants stay unchanged.[file:84]
