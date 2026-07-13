# Cyboquatic Scenario Profile (v1)

This document explains the Cyboquatic adversarial scenario used as an operational proof for Prometheus‑Praxis. It is the human‑readable counterpart to `aln/cyboquatic-scenario-profile.v1.aln` and the implementation in `crates/cyboquatic-runner`. [file:83]

The goal is to deterministically stress the governance stack (Lyapunov guard, KER enforcement, Cyboquatic index, role‑bands, shard bindings) and prove that it:

- Detects and rejects unsafe or unjustified behavior.
- Returns to a stable operating regime after shocks.
- Maintains bounded global metrics across the entire run. [file:83]

---

## Scenario structure

The scenario consists of `num_epochs` discrete steps. Each epoch is assigned to one of four phases:

- Warmup (Phase 0): safe baseline behavior.
- SafetyShock (Phase 1): deliberate safety envelope violations.
- DataLaborShock (Phase 2): attempts to gain value without data‑labor evidence.
- Recovery (Phase 3): safe, evidence‑backed improvements and stabilization. [file:83]

For reproducibility, all randomness uses a fixed PRNG seed (`StdRng::seed_from_u64(0xC0B0_0A71)`), making CI runs deterministic. [file:83]

---

## Agent and shard context

The scenario uses a single coordinator‑class agent:

- `agent_id`: `agent-coordinator-01`
- `role-band`: `Coordinator`
- `shard-id`: `Shard-3`
- `aln_shard_id`: `agent-coordinator-01.v1.aln` [file:83]

This must satisfy:

- Role‑band definition in `prometheus-role-bands.v1.aln`.
- Shard layout invariants in `prometheus-shard-layout.v1.aln`.
- `ShardBinding::is_allowed()` as implemented in `crates/agent-registry`. [file:83]

This ensures the stress test runs **inside** the same role‑band/shard regime that production agents use.

---

## Phase semantics

### Warmup (Phase 0)

- Epochs: first quarter of `num_epochs`.
- Behavior:
  - `KerSnapshot` values are near corridor midpoints.
  - `socio_ecolabour` is incremented slightly (`+0.02`), staying within `[0,1]`.
  - All safety envelopes (RoH ceiling, lifeforce floor, biocompatibility, pain, fear bands) remain satisfied.
  - Eco‑labour evidence is present and tethered. [file:83]

- Expectations:
  - `evaluate_ker_guard` should generally accept or flag only minor non‑monotone residuals.
  - Cyboquatic index should remain in a mid‑band around ~0.5. [file:83]

### SafetyShock (Phase 1)

- Epochs: second quarter of `num_epochs`.
- Behavior:
  - `rohscalar` is pushed above `ROH_CEILING_GLOBAL` (`+0.05`).
  - `lifeforcescalar` is pushed below `LIFEFORCE_FLOOR_GLOBAL` (`−0.05`).
  - Eco‑labour evidence is still present. [file:83]

- Expectations:
  - `evaluate_ker_guard` must return `RejectSafety` for these epochs.
  - Cyboquatic guard should also treat these states as unsafe.
  - Residuals can be positive during this phase; they are **not** required to be stable while shocks are active. [file:83]

### DataLaborShock (Phase 2)

- Epochs: third quarter of `num_epochs`.
- Behavior:
  - `socio_ecolabour` is increased more aggressively (`+0.1`) to simulate improvement.
  - `data_labor_density` is set to `0.0`.
  - Eco evidence is absent (`has_measurement_tethered = false`). [file:83]

- Expectations:
  - If `compute_cyboquatic_score` yields a higher index value than the prior epoch, `evaluate_cyboquatic_decision` must return `RejectNoDataLabor`.
  - Lyapunov guard may also produce `RejectDataLaborMissing` where residuals indicate improvement without evidence. [file:83]

This phase directly tests the **data‑as‑labour** invariants: no improvement is accepted without measurement‑tethered eco evidence.

### Recovery (Phase 3)

- Epochs: final quarter of `num_epochs`.
- Behavior:
  - `socio_ecolabour` is increased moderately (`+0.05`) with full eco evidence.
  - `rohscalar` is slightly reduced, `lifeforcescalar` increased, tightening envelopes.
  - `data_labor_density` is restored to `1.0`. [file:83]

- Expectations:
  - Lyapunov residuals must be `≤ 0.0` during this phase (no further divergence).
  - Cyboquatic index should recover to a healthy mean.
  - No excessive emergency triggers should be necessary. [file:83]

---

## Metrics and pass criteria

The runner records three main time series:

- `lyapunov_residual_series`: divergence/stability signal from the KER guard.
- `ker_fairness_series`: synthetic fairness ratio per epoch (currently `1.0` in safe phases, `0.8` in shocks).
- `cyboquatic_index_series`: scalar index in `[0,1]` computed from Cyboquatic inputs. [file:83]

From these, the following criteria are computed:

- Residual stability:
  - For all epochs in `Recovery`, residual `≤ 0.0`.
  - Rationale: the system must re‑enter a non‑divergent regime after shocks. [file:83]

- KER bounds:
  - For all epochs, `0.0 ≤ ker_fairness ≤ 1.5`.
  - Rationale: fairness signal must remain within defined corridors. [file:83]

- Cyboquatic index health:
  - `min_index ≥ 0.3` over the whole run.
  - `mean_index ≥ 0.5` over the whole run.
  - Rationale: the ecosystem cannot collapse to near‑zero health or hover permanently near failure. [file:83]

- Policy no‑panic:
  - `emergency_policy_triggers ≤ 8`.
  - Rationale: the governance stack may invoke guards in response to shocks, but must not oscillate into chronic “panic” mode. [file:83]

If all four criteria are satisfied, `pass_overall = true`. The CI pipeline treats any failure as a **deployment blocker**.

---

## Guard behavior expectations

During the scenario, we explicitly assert:

- In `DataLaborShock`:
  - Any index increase without data‑labour must produce `CyboquaticDecision::RejectNoDataLabor`. [file:83]

- In `SafetyShock`:
  - Safety envelope violations must yield `KerGuardDecision::RejectSafety` and/or `CyboquaticDecision::RejectSafety`. [file:83]

If these assertions fail, the test binary exits with a non‑zero code, causing `make governance-check` to fail.

---

## Auditor guidance

A passing Cyboquatic run demonstrates that:

- Role‑bands and shard bindings are enforced by construction for the tested agent.
- Safety envelopes and data‑as‑labour invariants cannot be bypassed by simple perturbations.
- The system absorbs defined shocks and returns to stable operation within bounded residual and index corridors. [file:83]

Auditors should:

- Compare this document with `aln/cyboquatic-scenario-profile.v1.aln` to confirm alignment between prose and formal specification.
- Inspect the Kani harnesses for `prometheus-praxis-lyapunov-guard` and `prometheus-praxis-cyboquatic` to see that ALN invariants are also proved at the crate level. [file:83][file:44]

For changes that materially alter the scenario (phases, thresholds, or internal weights), both the ALN shard and this document must be updated in the same commit, and the governance CI must still pass for the change to be admissible.
